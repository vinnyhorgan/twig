#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <miniz.h>

#define WIDTH 320
#define HEIGHT 240

typedef struct {
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  bool fullscreen;
  double current_time;
  double prev_time;
  lua_State* L;
  int update_ref;
  int draw_ref;
} State;

typedef struct {
  SDL_Surface* s;
  bool owned;
} Surface;

static void push_locked_surface(lua_State* L, SDL_Surface* s) {
  Surface* ud = (Surface*)lua_newuserdata(L, sizeof(Surface));
  ud->s = s;
  ud->owned = false;  // not owned by Lua, will not free
  luaL_getmetatable(L, "surface");
  lua_setmetatable(L, -2);
}

static int l_surface_gc(lua_State* L) {
  Surface* ud = (Surface*)luaL_checkudata(L, 1, "surface");
  if (ud->owned && ud->s) {
    SDL_DestroySurface(ud->s);
  }
  return 0;
}

static int l_surface_new(lua_State* L) {
  State* state = *((State**)lua_getextraspace(L));

  int width = (int)luaL_checkinteger(L, 1);
  int height = (int)luaL_checkinteger(L, 2);

  SDL_Surface* surface = SDL_CreateSurface(width, height, state->texture->format);
  if (!surface) {
    return luaL_error(L, "failed to create surface: %s", SDL_GetError());
  }

  Surface* ud = (Surface*)lua_newuserdata(L, sizeof(Surface));
  ud->s = surface;
  ud->owned = true;

  luaL_getmetatable(L, "surface");
  lua_setmetatable(L, -2);

  return 1;
}

static int l_surface_clear(lua_State* L) {
  Surface* ud = (Surface*)luaL_checkudata(L, 1, "surface");
  if (!ud->s) {
    return luaL_error(L, "invalid surface");
  }
  Uint8 r = (Uint8)luaL_checkinteger(L, 2);
  Uint8 g = (Uint8)luaL_checkinteger(L, 3);
  Uint8 b = (Uint8)luaL_checkinteger(L, 4);
  Uint8 a = (Uint8)luaL_optinteger(L, 5, SDL_ALPHA_OPAQUE);
  SDL_ClearSurface(ud->s, (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f);
  return 0;
}

static const luaL_Reg surface_funcs[] = {
  { "__gc", l_surface_gc },
  { "new", l_surface_new },
  { "clear", l_surface_clear },
  { NULL, NULL },
};

static int l_get_clipboard(lua_State* L) {
  char* text = SDL_GetClipboardText();
  if (!text)
    return 0;
  lua_pushstring(L, text);
  SDL_free(text);
  return 1;
}

static const luaL_Reg twig_funcs[] = {
  { "get_clipboard", l_get_clipboard },
  { NULL, NULL },
};

int luaopen_twig(lua_State* L) {
  luaL_newlib(L, twig_funcs);

  luaL_newmetatable(L, "surface");
  luaL_setfuncs(L, surface_funcs, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");

  lua_setfield(L, -2, "surface");
  return 1;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  SDL_SetAppMetadata("twig", "0.1", "com.vinnyhorgan.twig");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("failed to initialize sdl: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  State* state = (State*)SDL_calloc(1, sizeof(State));
  if (!state) {
    SDL_Log("failed to allocate state: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  *(State**)appstate = state;

  SDL_EnableScreenSaver();
  SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

  float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

  if (!SDL_CreateWindowAndRenderer("twig", 640 * scale, 480 * scale, SDL_WINDOW_RESIZABLE, &state->window,
                                   &state->renderer)) {
    SDL_Log("failed to create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_SetWindowMinimumSize(state->window, WIDTH, HEIGHT);
  SDL_SetRenderVSync(state->renderer, 1);
  SDL_SetRenderLogicalPresentation(state->renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STREAMING);
  SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, WIDTH);
  SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, HEIGHT);

  state->texture = SDL_CreateTextureWithProperties(state->renderer, props);
  if (!state->texture) {
    SDL_Log("failed to create texture: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_DestroyProperties(props);
  SDL_SetTextureScaleMode(state->texture, SDL_SCALEMODE_NEAREST);

  state->L = luaL_newstate();
  if (!state->L) {
    SDL_Log("failed to create lua state: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  *((State**)lua_getextraspace(state->L)) = state;

  luaL_openlibs(state->L);
  luaL_requiref(state->L, "twig", luaopen_twig, 1);

  char path[256];
  SDL_snprintf(path, sizeof(path), "%sdata/main.lua", SDL_GetBasePath());

  if (luaL_dofile(state->L, path) != LUA_OK) {
    SDL_Log("failed to run main.lua: %s", lua_tostring(state->L, -1));
    return SDL_APP_FAILURE;
  }

  lua_getglobal(state->L, "twig");
  lua_getfield(state->L, -1, "update");
  state->update_ref = lua_isfunction(state->L, -1) ? luaL_ref(state->L, LUA_REGISTRYINDEX) : LUA_NOREF;
  lua_getfield(state->L, -1, "draw");
  state->draw_ref = lua_isfunction(state->L, -1) ? luaL_ref(state->L, LUA_REGISTRYINDEX) : LUA_NOREF;
  lua_pop(state->L, 1);  // pop twig table

  state->current_time = SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  State* state = (State*)appstate;

  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  } else if (event->type == SDL_EVENT_KEY_DOWN) {
    if (event->key.key == SDLK_RETURN && event->key.mod & SDL_KMOD_ALT) {
      state->fullscreen = !state->fullscreen;
      SDL_SetWindowFullscreen(state->window, state->fullscreen);
    }
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  State* state = (State*)appstate;

  state->prev_time = state->current_time;
  state->current_time = SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
  double delta_time = state->current_time - state->prev_time;

  if (state->update_ref != LUA_NOREF) {
    lua_rawgeti(state->L, LUA_REGISTRYINDEX, state->update_ref);
    lua_pushnumber(state->L, delta_time);
    if (lua_pcall(state->L, 1, 0, 0) != LUA_OK) {
      SDL_Log("failed to call update function: %s", lua_tostring(state->L, -1));
      return SDL_APP_FAILURE;
    }
  }

  const double now = ((double)SDL_GetTicks()) / 1000.0;
  const float red = (float)(0.5 + 0.5 * SDL_sin(now));
  const float green = (float)(0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 2 / 3));
  const float blue = (float)(0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 4 / 3));

  SDL_Surface* s = NULL;
  if (!SDL_LockTextureToSurface(state->texture, NULL, &s)) {
    SDL_Log("failed to lock texture: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (state->draw_ref != LUA_NOREF) {
    lua_rawgeti(state->L, LUA_REGISTRYINDEX, state->draw_ref);
    push_locked_surface(state->L, s);
    if (lua_pcall(state->L, 1, 0, 0) != LUA_OK) {
      SDL_Log("failed to call draw function: %s", lua_tostring(state->L, -1));
      SDL_UnlockTexture(state->texture);
      return SDL_APP_FAILURE;
    }
  }

  SDL_UnlockTexture(state->texture);

  SDL_SetRenderDrawColorFloat(state->renderer, red, green, blue, SDL_ALPHA_OPAQUE_FLOAT); /* new color, full alpha. */
  SDL_RenderClear(state->renderer);
  SDL_RenderTexture(state->renderer, state->texture, NULL, NULL);
  SDL_RenderPresent(state->renderer);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  if (appstate) {
    State* state = (State*)appstate;
    lua_close(state->L);
    SDL_DestroyTexture(state->texture);
    SDL_DestroyRenderer(state->renderer);
    SDL_DestroyWindow(state->window);
    SDL_free(state);
  }
}
