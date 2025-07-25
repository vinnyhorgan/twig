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

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  SDL_SetAppMetadata("twig", "0.1", "com.vinnyhorgan.twig");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("failed to initialize sdl: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_CreateWindowAndRenderer("twig", 640, 480, 0, &window, &renderer)) {
    SDL_Log("failed to create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

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
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {}
