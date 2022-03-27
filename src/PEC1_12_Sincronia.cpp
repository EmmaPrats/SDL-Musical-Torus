#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <iostream>
#include <cmath>

//Screen dimension constants
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

//The window we'll be rendering to
SDL_Window* window = NULL;
//The surface contained by the window
SDL_Surface* screenSurface = NULL;

#define FPS 60
int lastTime = 0, currentTime, deltaTime;
float msFrame = 1 / (FPS / 1000.0f);

Uint32 Backgroundcolor;
SDL_Surface *flashTexture;

Mix_Music *mySong;
#define BPM_MUSIC 128
#define MSEG_BPM (60000 / BPM_MUSIC)
#define FLASH_MAX_TIME 300
int flashtime;
int MusicCurrentTime;
int MusicCurrentTimeBeat;
int MusicCurrentBeat;
int MusicPreviousBeat;

bool initSDL();
void update();
void render();

void close();
void waitTime();

void initMusic();
void updateMusic();
void renderMusic();
void renderFlash(SDL_Surface *surf, Uint8 alpha);


int main( int argc, char* args[] )
{
	//Start up SDL and create window
	if (!initSDL())
	{
		std::cout << "Failed to initialize!\n";
		return 1;
	}
	else
	{
		initMusic();

		//Main loop flag
		bool quit = false;

		//Event handler
		SDL_Event e;

		//While application is running
		while (!quit)
		{
			//Handle events on queue
			while (SDL_PollEvent(&e) != 0)
			{
				if (e.type == SDL_KEYDOWN) {
					if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
						quit = true;
					}
				}
				//User requests quit
				if (e.type == SDL_QUIT)
				{
					quit = true;
				}
			}

			// updates all
			update();

			//Render
			render();

			//Update the surface
			SDL_UpdateWindowSurface(window);
			waitTime();
		}
	}

	//Free resources and close SDL
	close();

	return 0;
}


bool initSDL() {

	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		std::cout << "SDL could not initialize! SDL_Error: %s\n" << SDL_GetError();
		return false;
	}
	//Create window
	window = SDL_CreateWindow("SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

	if (window == NULL)
	{
		std::cout << "Window could not be created! SDL_Error: %s\n" << SDL_GetError();
		return false;
	}
	//Get window surface
	screenSurface = SDL_GetWindowSurface(window);
	return true;
}

void update(){
	updateMusic();
}

void render() {

	renderMusic();
}

void close() {
	Mix_HaltMusic();
	Mix_FreeMusic(mySong);
	Mix_Quit();
	Mix_CloseAudio();
	//Destroy window
	SDL_DestroyWindow(window);
	//Quit SDL subsystems
	SDL_Quit();
}

void waitTime() {
	currentTime = SDL_GetTicks();
	deltaTime = currentTime - lastTime;
	if (deltaTime < (int)msFrame) {
		SDL_Delay((int)msFrame - deltaTime);
	}
	lastTime = currentTime;

}

void initMusic() {
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
	Mix_Init(MIX_INIT_OGG);
	mySong =  Mix_LoadMUS("resources/Blastculture-Gravitation.ogg");
	if (!mySong) {
		std::cout << "Error loading Music: " << Mix_GetError() << std::endl;
		close();
		exit(1);
	}
	Mix_PlayMusic(mySong,0);
	flashtime = 0;
	MusicCurrentTime = 0;
	MusicCurrentTimeBeat = 0;
	MusicCurrentBeat = 0;
	MusicPreviousBeat = -1;
	Backgroundcolor = 0xFF000000 | ((rand() % 256) << 16) | ((rand() % 256) << 8) | (rand() % 256);
	// load the texture
	SDL_Surface* temp = IMG_Load("resources/texture_torus.png");
	if (temp == NULL) {
		std::cout << "Image can be loaded! " << IMG_GetError();
		close();
		exit(1);
	}
	flashTexture = SDL_ConvertSurfaceFormat(temp, SDL_PIXELFORMAT_ARGB8888, 0);

}

void updateMusic() {
	MusicCurrentTime += deltaTime;
	MusicCurrentTimeBeat += deltaTime;
	MusicPreviousBeat = MusicCurrentBeat;
	if (MusicCurrentTimeBeat >= MSEG_BPM) {
		MusicCurrentTimeBeat = 0;
		MusicCurrentBeat ++;
		flashtime = FLASH_MAX_TIME;
	}
	if (flashtime > 0) {
		flashtime -= deltaTime;
	}
	else {
		flashtime = 0;
	}
	if (!Mix_PlayingMusic()) {
		close();
		exit(0);
	}
}

void renderMusic() {
	std::cout << "Beat Time: " << MusicCurrentTimeBeat;
	std::cout << "\tBeat: " << MusicCurrentBeat;
	std::cout << "\tFlash Time: " << flashtime;
	std::cout << "\tMusic Time: " << MusicCurrentTime;
	std::cout << std::endl;

	if (MusicPreviousBeat != MusicCurrentBeat) {
		Backgroundcolor = 0xFF000000 | ((rand() % 256) << 16) | ((rand() % 256) << 8) | (rand() % 256);
	}
	SDL_FillRect(screenSurface, NULL, Backgroundcolor);

	if (MusicCurrentTime > 9350) {
		if (flashtime > 0) {
			renderFlash(flashTexture, (Uint8)(255 * flashtime / (float)(FLASH_MAX_TIME)));
		}
	}
}

void renderFlash(SDL_Surface *surf, Uint8 alpha) {

	SDL_SetSurfaceAlphaMod(surf, alpha);
	SDL_BlitSurface(surf,NULL, screenSurface, NULL);
	SDL_SetSurfaceAlphaMod(surf, 255);
}

