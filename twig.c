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
} State;

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

  SDL_Log("%s", SDL_GetPixelFormatName(state->texture->format));

  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  luaL_loadstring(L, "print('hello!')");
  if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
    SDL_Log("lua error: %s", lua_tostring(L, -1));
    lua_close(L);
    return SDL_APP_FAILURE;
  }

  const char* input = "miniz!";
  char compressed[100], decompressed[100];
  mz_ulong comp_len = sizeof(compressed);
  compress((unsigned char*)compressed, &comp_len, (const unsigned char*)input, strlen(input));

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  State* state = (State*)appstate;

  const double now = ((double)SDL_GetTicks()) / 1000.0;
  const float red = (float)(0.5 + 0.5 * SDL_sin(now));
  const float green = (float)(0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 2 / 3));
  const float blue = (float)(0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 4 / 3));

  SDL_Surface* s = NULL;
  if (!SDL_LockTextureToSurface(state->texture, NULL, &s)) {
    SDL_Log("failed to lock texture: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_ClearSurface(s, 1, 0, 0, 1);
  SDL_Rect rect = { 10, 10, 50, 50 };
  SDL_FillSurfaceRect(s, &rect, SDL_MapRGB(SDL_GetPixelFormatDetails(s->format), NULL, 255, 255, 0));
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
    SDL_DestroyTexture(state->texture);
    SDL_DestroyRenderer(state->renderer);
    SDL_DestroyWindow(state->window);
    SDL_free(state);
  }
}
