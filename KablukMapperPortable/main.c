#include <stdio.h>
#include <Windows.h>
#include <SDL.h>

int main(int argc, char* argv[]) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("SDL initializing error: %s\n", SDL_GetError());
	}

SDL_Window* window = SDL_CreateWindow(
	"Kabluk Mapper Portable",
	SDL_WINDOWPOS_CENTERED,
	SDL_WINDOWPOS_CENTERED,
	480, 272,
	SDL_WINDOW_SHOWN);

	if (!window) {
		printf("window not created: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
}

int running = 1;
SDL_Event event;
while (running) {
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {

		}
	}
}

SDL_DestroyWindow(window);
SDL_Quit();
return 0;
}