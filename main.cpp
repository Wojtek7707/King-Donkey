#define _USE_MATH_DEFINES
#include<math.h>
#include<stdio.h>
#include<string.h>

extern "C" {
#include"./SDL2-2.0.10/include/SDL.h"
#include"./SDL2-2.0.10/include/SDL_main.h"
}

#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480
#define CHAR_SIZE 8 // rozmiar znaku w bitmapie
#define CHARACTER_WIDTH 32
#define CHARACTER_HEIGHT 32
#define PLATFORM_HEIGHT 20
#define LADDER_WIDTH 40
#define LADDER_HEIGHT 100
#define SPEED 3
#define GRAVITY 0.5f
#define NUM_BARRELS 3 // Iloœæ beczek w grze

typedef struct {
	int x, y, w, h;
	int dx, dy;// pozycja beczki
	int speedX, speedY; // prêdkoœæ beczki
	SDL_Texture* sprite; // grafika beczki
} Barrel;
typedef struct {
	char playerName[50]; // Maksymalna d³ugoœæ nazwy gracza to 50 znaków
} Player;
// struktura reprezentuj¹ca platformê
typedef struct {
	int x, y, w, h; // pozycja i rozmiar platformy
} Platform;

typedef struct {
	int x, y, w, h; // pozycja i rozmiar drabinki
} LadderPlatform;

typedef struct {
	int x, y, w, h;// pozycja i rozmiar drabinki
	SDL_Texture* sprite;
} Water;

typedef struct {
	int x, y; // pozycja postaci
	int dx, dy; // kierunek poruszania siê postaci
	int speedX, speedY;
	int jumping; // czy postaæ skacze
	int lives;// liczba ¿yæ
	SDL_Texture* sprite; // grafika postaci
} Character;

typedef struct {
	int x, y; // pozycja drabinki
	SDL_Texture* sprite; // grafika drabinki
} Ladder;

struct GameContext {
	SDL_Renderer* renderer;
	SDL_Texture* charsetTexture;
	SDL_Texture* characterTexture;
	SDL_Texture* ladderTexture;
	SDL_Texture* waterTexture;
	Character* character;
	int num_ladders;
	Platform* platforms;
	int num_platforms;
	Water* water;
	int* currentStage;
	Uint32* time;
	Player* player;
	LadderPlatform* ladderplatform;
	Ladder* ladders;
	int* gameRunning;
	int delta;
	Barrel* barrel;
	SDL_Texture* barrelTexture;
};


// narysowanie napisu txt na powierzchni screen, zaczynaj¹c od punktu (x, y)
// charset to bitmapa 128x128 zawieraj¹ca znaki
// draw a text txt on surface screen, starting from the point (x, y)
// charset is a 128x128 bitmap containing character images
void DrawString(SDL_Surface* screen, int x, int y, const char* text,
	SDL_Surface* charset) {
	int px, py, c;
	SDL_Rect s, d;
	s.w = 8;
	s.h = 8;
	d.w = 8;
	d.h = 8;
	while (*text) {
		c = *text & 255;
		px = (c % 16) * 8;
		py = (c / 16) * 8;
		s.x = px;
		s.y = py;
		d.x = x;
		d.y = y;
		SDL_BlitSurface(charset, &s, screen, &d);
		x += 8;
		text++;
	};
};

// Rysowanie linii
void DrawLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	SDL_SetRenderDrawColor(renderer, r, g, b, a);
	SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void DrawRectangle(SDL_Renderer* renderer, int x, int y, int l, int k, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = l;
	rect.h = k;
	SDL_SetRenderDrawColor(renderer, r, g, b, a);
	SDL_RenderFillRect(renderer, &rect);
}
void DrawWater(SDL_Renderer* renderer, Water* water) {
	SDL_Rect dest;
	dest.x = water->x; // szerokoœæ sprite'a / 2
	dest.y = water->y; // wysokoœæ sprite'a / 2
	dest.w = water->w; // szerokoœæ sprite'a
	dest.h = water->h; // wysokoœæ sprite'a
	SDL_RenderCopy(renderer, water->sprite, NULL, &dest);
}
void DrawBarrel(SDL_Renderer* renderer, Barrel* barrel) {
	SDL_Rect dest;
	dest.x = barrel->x;
	dest.y = barrel->y;
	dest.w = 32;// szerokoœæ beczki
	dest.h = 32;// wysokoœæ beczki
	SDL_RenderCopy(renderer, barrel->sprite, NULL, &dest);
}
// Sprawdza, czy beczka jest na platformie
int IsOnPlatformBarrel(Barrel* barrel, Platform* platforms, int num_platforms) {
	for (int i = 0; i < num_platforms; i++) {
		if ((barrel->x - barrel->w < platforms[i].x + platforms[i].w &&
			barrel->x + barrel->w > platforms[i].x &&
			barrel->y - barrel->h < platforms[i].y + platforms[i].h &&
			barrel->y + barrel->h > platforms[i].y)) {
			return 1;
		}
	}
	return 0;
}
// Sprawdza kolizje boczne beczki z platformami
int CheckSideCollisionBarrel(Barrel* barrel, Platform* platforms, int num_platforms) {
	for (int i = 0; i < num_platforms; i++) {
		// Sprawdzanie kolizji z praw¹ krawêdzi¹ platformy
		if (barrel->x > platforms[i].x &&
			barrel->x - barrel->w < platforms[i].x &&
			barrel->y > platforms[i].y &&
			barrel->y - barrel->h < platforms[i].y + PLATFORM_HEIGHT) {
			return 1;
		}
		// Sprawdzanie kolizji z lew¹ krawêdzi¹ platformy
		if (barrel->x - barrel->w < platforms[i].x + platforms[i].w &&
			barrel->x > platforms[i].x + platforms[i].w &&
			barrel->y > platforms[i].y &&
			barrel->y - barrel->h < platforms[i].y + PLATFORM_HEIGHT) {
			return 1;
		}
	}
	return 0;
}
int CheckBarrelCollision(Character* character, Barrel* barrel) {
	if (character->x < barrel->x + barrel->w &&
		character->x + CHARACTER_WIDTH > barrel->x &&
		character->y < barrel->y + barrel->h &&
		character->y + CHARACTER_HEIGHT> barrel->y) {
		return 1; // Kolizja z beczk¹
	}
	return 0;
}
// Sprawdza kolizjê miêdzy beczk¹ a postaci¹
void UpdateBarrel(Barrel* barrel, int czas, Character* character, Platform* platforms, int num_platforms) {
	int old_x = barrel->x;
	int old_y = barrel->y;
	barrel->dx += barrel->speedX * czas;
	barrel->dy += barrel->speedY * czas;

	barrel->x += barrel->dx;
	barrel->y += barrel->dy;

	// Sprawdzanie kolizji z platformami
	if (IsOnPlatformBarrel(barrel, platforms, num_platforms)) {
		barrel->y = old_y;
		barrel->dy = 0;
	}
	barrel->dy += 1;
	// Sprawdzanie kolizji bocznych dla ka¿dej platformy
	for (int i = 0; i < num_platforms; i++) {
		if (CheckSideCollisionBarrel(barrel, platforms, num_platforms)) {
			barrel->x = old_x; // Jeœli wyst¹pi³a kolizja boczna, cofnij ruch w poziomie
			barrel->dx *= -1; // Odwróæ kierunek ruchu
		}
	}
	if (CheckBarrelCollision(character, barrel)) {
		character->lives -= 1;
		character->x = 40;
		character->y = 420;
	}
	if (barrel->x <= 0) {
		barrel->x = 0;
		barrel->dx *= -1; // Odwróæ kierunek ruchu
	}
	if (barrel->x >= SCREEN_WIDTH - CHARACTER_WIDTH - 50) {
		barrel->x = SCREEN_WIDTH - CHARACTER_WIDTH - 50;
		barrel->dx *= -1; // Odwróæ kierunek ruchu
	}
	if (barrel->x <= 100) {
		barrel->x = 100;
		barrel->dx *= -1;
	}
	if (barrel->x <= 100 && barrel->y >= 400) {
		barrel->x = 600;
		barrel->y = 100;
		//barrel->dx *= -1;
	}

}

// Rysowanie tekstu na ekranie
void DrawText(SDL_Renderer* renderer, SDL_Texture* charset, const char* text, int x, int y) {
	SDL_Rect src, dest;
	src.w = CHAR_SIZE;
	src.h = CHAR_SIZE;
	dest.w = CHAR_SIZE;
	dest.h = CHAR_SIZE;

	while (*text) {
		char c = *text;
		src.x = (c % 16) * CHAR_SIZE;
		src.y = (c / 16) * CHAR_SIZE;
		dest.x = x;
		dest.y = y;
		SDL_RenderCopy(renderer, charset, &src, &dest);
		x += CHAR_SIZE;
		text++;
	}
}

// Inicjalizacja postaci
void InitCharacter(Character* character, SDL_Texture* sprite, int x, int y) {
	character->x = x;
	character->y = y;
	character->dx = 0;
	character->dy = 0;
	character->speedX = 0;
	character->speedY = 0;
	character->jumping = 0; // Dodaj tê liniê
	character->lives = 3;
	character->sprite = sprite;
}
void InitRadder(Ladder* ladder, SDL_Texture* sprite, int x, int y) {
	ladder->x = x;
	ladder->y = y;
	ladder->sprite = sprite;
}

void InitWater(Water* water, SDL_Texture* sprite, int x, int y, int w, int h) {
	water->x = x;
	water->y = y;
	water->w = w;
	water->h = h;
	water->sprite = sprite;
}
void InitBarrel(Barrel* barrel, SDL_Texture* sprite, int x, int y, int w, int h, int speedX, int speedY) {
	barrel->x = x;
	barrel->y = y;
	barrel->w = w;
	barrel->h = h;
	barrel->dx = -1;
	barrel->dy = 1;
	barrel->speedX = speedX;
	barrel->speedY = speedY;
	barrel->sprite = sprite;
}
// Rysowanie postaci
void DrawCharacter(SDL_Renderer* renderer, Character* character) {
	SDL_Rect dest;
	dest.x = character->x - 32; // szerokoœæ sprite'a / 2
	dest.y = character->y - 32; // wysokoœæ sprite'a / 2
	dest.w = 32; // szerokoœæ sprite'a
	dest.h = 32; // wysokoœæ sprite'a
	SDL_RenderCopy(renderer, character->sprite, NULL, &dest);
}
void DrawRadder(SDL_Renderer* renderer, Ladder* ladder) {
	SDL_Rect dest;
	dest.x = ladder->x; // szerokoœæ sprite'a / 2
	dest.y = ladder->y; // wysokoœæ sprite'a / 2
	dest.w = LADDER_WIDTH; // szerokoœæ sprite'a
	dest.h = LADDER_HEIGHT; // wysokoœæ sprite'a
	SDL_RenderCopy(renderer, ladder->sprite, NULL, &dest);
}

// Sprawdzanie kolizji bocznych dla ka¿dej platformy
int CheckSideCollision(Character* character, Platform* platforms, int num_platforms) {
	for (int i = 0; i < num_platforms; i++) {
		// Sprawdzanie kolizji z praw¹ krawêdzi¹ platformy
		if (character->x > platforms[i].x &&
			character->x - CHARACTER_WIDTH < platforms[i].x &&
			character->y > platforms[i].y &&
			character->y - CHARACTER_HEIGHT < platforms[i].y + PLATFORM_HEIGHT) {
			return 1;
		}
		// Sprawdzanie kolizji z lew¹ krawêdzi¹ platformy
		if (character->x - CHARACTER_WIDTH < platforms[i].x + platforms[i].w &&
			character->x > platforms[i].x + platforms[i].w &&
			character->y > platforms[i].y &&
			character->y - CHARACTER_HEIGHT < platforms[i].y + PLATFORM_HEIGHT) {
			return 1;
		}
	}
	return 0;
}
int CheckWaterCollision(Character* character, Water* water) {
	if (character->x < water->x + water->w &&
		character->x  > water->x &&
		character->y < water->y + water->h &&
		character->y  > water->y) {
		return 1; // Kolizja z wod¹
	}
	return 0;
}
// Sprawdzenie, czy postaæ jest na platformie
int IsOnPlatform(Character* character, Platform* platforms, int num_platforms) {
	for (int i = 0; i < num_platforms; i++) {
		if ((character->x - CHARACTER_WIDTH < platforms[i].x + platforms[i].w &&
			character->x > platforms[i].x &&
			character->y - CHARACTER_HEIGHT < platforms[i].y + platforms[i].h &&
			character->y > platforms[i].y)) {
			return 1;
		}
	}
	return 0;
}
int IsPlatform(Character* character, Platform* platforms, int num_platforms) {
	for (int i = 0; i < num_platforms; i++) {
		if ((character->x - CHARACTER_WIDTH < platforms[i].x + platforms[i].w &&
			character->x + 1 > platforms[i].x &&
			character->y - CHARACTER_HEIGHT < platforms[i].y + platforms[i].h &&
			character->y + 1 > platforms[i].y)) {
			return 1;
		}
	}
	return 0;
}

// Sprawdzenie, czy postaæ jest na drabince
int IsOnLadder(Character* character, LadderPlatform* ladders, int num_ladders) {
	for (int i = 0; i < num_ladders; i++) {
		if (character->x > ladders[i].x + 20 &&
			character->x - CHARACTER_WIDTH < ladders[i].x + 20 &&
			character->y > ladders[i].y &&
			character->y - CHARACTER_HEIGHT + 10 < ladders[i].y + LADDER_HEIGHT) {
			return 1; // Postaæ jest na drabinie
		}
	}
	return 0;
}
void GameOver(SDL_Renderer* renderer, SDL_Texture* charsetTexture) {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	DrawText(renderer, charsetTexture, "Game Over!", 250, 200);
	DrawText(renderer, charsetTexture, "Nacisnij 't' zeby kontynuowac. Nacisnij 'n' ¿eby wyjsc.", 180, 250);
	SDL_RenderPresent(renderer);

	SDL_Event event;
	int waiting = 1;
	while (waiting) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				exit(0);
			}
			if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
				case SDLK_n:
					exit(0);
					break;
				case SDLK_t:
					waiting = 0;
					break;
				}
			}
		}
	}
}
void ShowMenu(SDL_Renderer* renderer, SDL_Texture* charsetTexture, Player* player) {
	DrawRectangle(renderer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 99, 99, 255);
	DrawRectangle(renderer, 100, 80, 420, 380, 0, 0, 0, 255);
	DrawText(renderer, charsetTexture, "MENU STARTOWE", 250, 100);
	DrawText(renderer, charsetTexture, "Zrobilem czesc podstawowa", 200, 120);
	DrawText(renderer, charsetTexture, "Z czesci nieobowiazkowej", 200, 140);
	DrawText(renderer, charsetTexture, "A B D oraz H", 200, 160);
	DrawText(renderer, charsetTexture, "Nacisnij ENTER aby zaczac", 200, 200);
	DrawText(renderer, charsetTexture, "Nacisnij 1 aby zaczac 1 poziom", 200, 240);
	DrawText(renderer, charsetTexture, "Nacisnij 2 aby zaczac 2 poziom", 200, 280);
	DrawText(renderer, charsetTexture, "Nacisnij 3 aby zaczac 3 poziom", 200, 320);
	DrawText(renderer, charsetTexture, "Nacisnij x, aby sprawdzic wyniki", 200, 360);
	DrawText(renderer, charsetTexture, "(niedostepne)", 200, 370);
	DrawText(renderer, charsetTexture, "Nacisnij ESC, aby wyjsc", 200, 400);


	// Wyœwietlanie pola do wpisywania nazwy gracza
	DrawText(renderer, charsetTexture, "Wpisz nazwe gracza:", 200, 420);
	DrawText(renderer, charsetTexture, player->playerName, 200, 430);
}

// Aktualizacja pozycji postaci
void UpdateCharacter(GameContext c) {
	int old_x = c.character->x;
	int old_y = c.character->y;
	c.character->dx += c.character->speedX * c.delta;
	c.character->dy += c.character->speedY * c.delta;

	c.character->x += c.character->dx;
	c.character->y += c.character->dy;

	// Dodaj obs³ugê skoku
	if (c.character->jumping) {
		c.character->dy += GRAVITY; // Zastosuj grawitacjê podczas skoku
		if (c.character->y > SCREEN_HEIGHT) {
			// Jeœli postaæ opuœci ekran, zakoñcz skok
			c.character->jumping = 0;
		}
	}
	// Sprawdzanie kolizji z platformami i drabinkami
	if (IsOnPlatform(c.character, c.platforms, c.num_platforms) && IsOnLadder(c.character, c.ladderplatform, c.num_ladders)) {
		c.character->dx = 0;
		c.character->dy = 0;
	}
	else if (IsOnPlatform(c.character, c.platforms, c.num_platforms)) {
		c.character->y = old_y;
		c.character->dy = 0;
	}
	else if (IsOnLadder(c.character, c.ladderplatform, c.num_ladders)) {
		c.character->x = old_x;
		c.character->dx = 0;
	}
	else if (CheckWaterCollision(c.character, c.water)) {
		// Kolizja z wod¹, odejmowanie ¿ycia
		c.character->lives--;
		if (c.character->lives > 0) {
			// Jeœli pozosta³y ¿ycia, zresetuj pozycjê postaci
			c.character->x = 40;
			c.character->y = 420;
		}
	}
	else {
		c.character->dy += 1;
	}

	// Sprawdzanie kolizji bocznych dla ka¿dej platformy
	for (int i = 0; i < c.num_platforms; i++) {
		if (CheckSideCollision(c.character, c.platforms, c.num_platforms)) {
			c.character->x = old_x; // Jeœli wyst¹pi³a kolizja boczna, cofnij ruch w poziomie
		}
	}
	if (c.character->x - CHARACTER_WIDTH < 0 + 200 && c.character->x + 1 > 0 && c.character->y - CHARACTER_HEIGHT < 100 + PLATFORM_HEIGHT &&
		c.character->y + 1 > 100 && *(c.currentStage) == 1) {
		// Zmiana na poziom 2
		*(c.currentStage)= 2;

		// Resetuj pozycjê postaci na pocz¹tek poziomu 2
		InitCharacter(c.character, c.character->sprite, 40, 420);
	}
	if (c.character->x - CHARACTER_WIDTH < 0 + 150 && c.character->x + 1 > 0 && c.character->y - CHARACTER_HEIGHT < 100 + PLATFORM_HEIGHT &&
		c.character->y + 1 > 100 && *(c.currentStage) == 2) {
		// Zmiana na poziom 3
		*(c.currentStage) = 3;

		// Resetuj pozycjê postaci na pocz¹tek poziomu 2
		InitCharacter(c.character, c.character->sprite, 40, 420);
		InitBarrel(c.barrel, c.barrel->sprite, 600, 100, 32, 32, -1, 1);
	}
	if (c.character->x - CHARACTER_WIDTH < 600 + 40 && c.character->x + 1 > 600 && c.character->y - CHARACTER_HEIGHT < 100 + PLATFORM_HEIGHT &&
		c.character->y + 1 > 100 && *(c.currentStage) == 3) {
		// Zmiana na menu
		*(c.currentStage) = 0;

		// Resetuj pozycjê postaci na pocz¹tek poziomu 2
		InitCharacter(c.character, c.character->sprite, 40, 420);
	}
	// Ograniczenie pozycji postaci do rozdzielczoœci gry
	if (c.character->x < CHARACTER_WIDTH) c.character->x = CHARACTER_WIDTH;
	if (c.character->y < CHARACTER_HEIGHT) c.character->y = CHARACTER_HEIGHT;
	if (c.character->x > SCREEN_WIDTH) c.character->x = SCREEN_WIDTH;
	if (c.character->y > SCREEN_HEIGHT) c.character->y = SCREEN_HEIGHT;
}
void level(GameContext& c) {

	SDL_Event event;
	SDL_SetRenderDrawColor(c.renderer, 0, 0, 0, 255);
	SDL_RenderClear(c.renderer);

	DrawCharacter(c.renderer, c.character);

	Uint32 gameTime = SDL_GetTicks() - *(c.time);
	float seconds = gameTime / 1000.0f;
	char gameTimeText[64];
	sprintf(gameTimeText, "Czas gry: %.2f s", seconds);
	DrawText(c.renderer, c.charsetTexture, gameTimeText, 20, 20);

	DrawRectangle(c.renderer, 0, 0, 640, 60, 0, 99, 99, 255);
	if (*(c.currentStage) == 1) {
		DrawRectangle(c.renderer, 0, 100, 200, PLATFORM_HEIGHT, 0, 99, 99, 255);
	}
	if (*(c.currentStage) == 2)
	{
		DrawRectangle(c.renderer, 0, 100, 150, PLATFORM_HEIGHT, 0, 99, 99, 255);
	}
	if (*(c.currentStage) == 3)
	{
		DrawRectangle(c.renderer, 600, 100, 40, PLATFORM_HEIGHT, 0, 99, 99, 255);
	}
	
	DrawText(c.renderer, c.charsetTexture, gameTimeText, 20, 20);
	DrawText(c.renderer, c.charsetTexture, c.player->playerName, 200, 20);

	DrawText(c.renderer, c.charsetTexture, "Liczba zyc:", 300, 20);
	for (int i = 0; i < 3; ++i) {
		if (i < c.character->lives) {
			DrawRectangle(c.renderer, 400 + i * 40, 10, 30, 30, 255, 0, 0, 255);
		}
		else {
			DrawRectangle(c.renderer, 400 + i * 40, 10, 30, 30, 0, 0, 0, 255);
		}
	}
	for (int i = 0; i < c.num_platforms; i++) {
		DrawRectangle(c.renderer, c.platforms[i].x, c.platforms[i].y, c.platforms[i].w, c.platforms[i].h, 165, 42, 42, 255);
	}

	for (int i = 0; i < c.num_ladders; i++) {
		DrawRadder(c.renderer, &(c.ladders[i]));
	}
	if (c.water != nullptr) {
		DrawWater(c.renderer, c.water);
	}
	if (*(c.currentStage) == 3) {
		DrawBarrel(c.renderer, c.barrel);
	}
	SDL_Delay(16);
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			*(c.gameRunning) = 0;
		}

		if (event.type == SDL_KEYDOWN) {
			switch (event.key.keysym.sym) {
			case SDLK_ESCAPE:
				*(c.gameRunning) = 0;
				break;
			case SDLK_n:
				*(c.time) = SDL_GetTicks();
				InitCharacter(c.character, c.characterTexture, 40, 420);
				break;
			case SDLK_LEFT:
				if (*c.currentStage==3 && IsPlatform(c.character, c.platforms, c.num_platforms))
				{
					c.character->dy = -2.5;
				}
				c.character->dx = -SPEED;
				break;
			case SDLK_RIGHT:
				if (*(c.currentStage) == 3 &&IsPlatform(c.character, c.platforms, c.num_platforms))
				{
					c.character->dy = -2.5;
				}
				c.character->dx = SPEED;
				break;
			case SDLK_UP:
				if (IsOnLadder(c.character, c.ladderplatform, c.num_ladders)) {
					c.character->dy = -SPEED;
				}
				else {
					c.character->dy = 0;
				}
				break;
			case SDLK_DOWN:
				if (IsOnLadder(c.character, c.ladderplatform, c.num_ladders)) {
					c.character->dy = SPEED;
				}
				else {
					c.character->dy = 0;
				}
				break;
			case SDLK_SPACE:
				if (!c.character->jumping && IsPlatform(c.character, c.platforms, c.num_platforms) && 
					!IsOnLadder(c.character, c.ladderplatform, c.num_ladders)) {
					c.character->dy = -20;
					c.character->jumping = 1;
				}
				break;
			case SDLK_1:
				*(c.currentStage) = 1;
				*(c.time) = SDL_GetTicks();
				InitCharacter(c.character, c.characterTexture, 40, 420);
				break;
			case SDLK_2:
				*(c.currentStage) = 2;
				*(c.time) = SDL_GetTicks();
				InitCharacter(c.character, c.characterTexture, 40, 420);
				break;
			case SDLK_3:
				*(c.currentStage) = 3;
				*(c.time) = SDL_GetTicks();
				InitCharacter(c.character, c.characterTexture, 40, 420);
				InitBarrel(c.barrel, c.barrelTexture, 600, 100, 32, 32, -1, 1);
				break;
			default:
				break;
			}
		}

		if (event.type == SDL_KEYUP) {
			switch (event.key.keysym.sym) {
			case SDLK_LEFT: if (c.character->dx < 0) c.character->dx = 0; break;
			case SDLK_RIGHT: if (c.character->dx > 0) c.character->dx = 0; break;
			case SDLK_UP:
			case SDLK_DOWN: c.character->dy = 0; break;
			case SDLK_SPACE: c.character->jumping = 0; break;
			default:
				break;
			}
		}
	}

	UpdateCharacter(c);
	if (*(c.currentStage) == 3) {
		UpdateBarrel(c.barrel, c.delta, c.character, c.platforms, c.num_platforms);
	}

	if (c.character->lives <= 0) {
		GameOver(c.renderer, c.charsetTexture);
		*(c.currentStage) = 0;
	}
}
void InitPlatform3(Platform* platforms3, int num_platforms3){

	int startX = 0;
	int startY = 460;
	for (int i = 0; i < 32; i++) {
		platforms3[i].x = startX;
		platforms3[i].y = startY;
		platforms3[i].h = PLATFORM_HEIGHT;
		platforms3[i].w = 20;
		startX += 20;
		startY -= 1;
	}

	startX = 500;
	startY = 340;
	for (int i = 32; i < 60; i++) {
		platforms3[i].x = startX;
		platforms3[i].y = startY;
		platforms3[i].h = PLATFORM_HEIGHT;
		platforms3[i].w = 20;
		startX -= 20;
		startY -= 1;
	}

	startX = 140;
	startY = 220;
	for (int i = 60; i < num_platforms3; i++) {
		platforms3[i].x = startX;
		platforms3[i].y = startY;
		platforms3[i].h = PLATFORM_HEIGHT;
		platforms3[i].w = 20;
		startX += 20;
		startY -= 1;
	}
}

int main(int argc, char** argv) {
	SDL_Init(SDL_INIT_EVERYTHING);

	SDL_Window* window = SDL_CreateWindow("King Donkey", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	SDL_Surface* characterSurface = SDL_LoadBMP("ball.bmp");
	SDL_Texture* characterTexture = SDL_CreateTextureFromSurface(renderer, characterSurface);
	SDL_FreeSurface(characterSurface);

	SDL_Surface* charsetSurface = SDL_LoadBMP("cs8x8.bmp");
	SDL_Texture* charsetTexture = SDL_CreateTextureFromSurface(renderer, charsetSurface);
	SDL_FreeSurface(charsetSurface);

	SDL_Surface* ladderSurface = SDL_LoadBMP("drabinka.bmp");
	SDL_Texture* ladder_texture = SDL_CreateTextureFromSurface(renderer, ladderSurface);
	SDL_FreeSurface(ladderSurface);

	SDL_Surface* waterSurface = SDL_LoadBMP("WaterGame.bmp");
	SDL_Texture* water_texture = SDL_CreateTextureFromSurface(renderer, waterSurface);
	SDL_FreeSurface(waterSurface);

	SDL_Surface* barrelSurface = SDL_LoadBMP("barrel.bmp");
	SDL_Texture* barrelTexture = SDL_CreateTextureFromSurface(renderer, barrelSurface);
	SDL_FreeSurface(barrelSurface);

	Character character;
	InitCharacter(&character, characterTexture, 40, 440);

	Ladder ladder1, ladder2, ladder3;
	InitRadder(&ladder1, ladder_texture, 400, 160);
	InitRadder(&ladder2, ladder_texture, 300, 260);
	InitRadder(&ladder3, ladder_texture, 440, 360);
	Ladder laddersInit[] = { ladder1, ladder2, ladder3 }, laddersInit2;
	Water water2, water1;
	InitWater(&water1, water_texture, 0, 0, 0, 0);
	InitWater(&water2, water_texture, 200, 460, 440, 20);

	Uint32 gameTime, gameStartTime = SDL_GetTicks();
	float seconds;
	char gameTimeText[64];
	SDL_Event event;
	int gameRunning = 1, currentStage = 0, t1 = SDL_GetTicks(), t2 = 0, startX, startY;	
	double delta, worldTime;

	Platform platforms[] = {
		{0, SCREEN_HEIGHT - 20, SCREEN_WIDTH, PLATFORM_HEIGHT},
		{270, 260, 400, PLATFORM_HEIGHT},
		{200, 360, 400, PLATFORM_HEIGHT},
		{100, 160, 400, PLATFORM_HEIGHT}
	};

	// Tablica drabinek
	LadderPlatform ladders[] = {
		{400, 140, LADDER_WIDTH, LADDER_HEIGHT},
		{300, 240, LADDER_WIDTH, LADDER_HEIGHT},
		{440, 340, LADDER_WIDTH, LADDER_HEIGHT}
	},ladders2[32], ladders3[32];
	
	const int num_platforms = sizeof(platforms) / sizeof(Platform);
	const int num_ladders = sizeof(ladders) / sizeof(LadderPlatform);

	Platform platforms2[] = {
		{0,SCREEN_HEIGHT - 20, 200, PLATFORM_HEIGHT},
		{210, 400, 50, PLATFORM_HEIGHT},
		{340, 400, 50, PLATFORM_HEIGHT},
		{470, 400, 50, PLATFORM_HEIGHT},
		{570, 300, 50, PLATFORM_HEIGHT},
		{470, 200, 50, PLATFORM_HEIGHT},
		{340, 200, 50, PLATFORM_HEIGHT},
		{210, 200, 50, PLATFORM_HEIGHT},
	};

	const int num_platforms2 = sizeof(platforms2) / sizeof(Platform), num_platforms3 = 88;
	const int num_ladders2 = 0, num_ladders3 = 0;

	Platform platforms3[num_platforms3];
	InitPlatform3(platforms3, num_platforms3);
	Barrel barrel; GameContext context; Player player;
	InitBarrel(&barrel, barrelTexture, 600, 100, 32, 32, -1, 1);
	strcpy(player.playerName, ""); // Inicjalizacja nazwy gracza jako pustego ci¹gu

	while (gameRunning) {
		t2 = SDL_GetTicks();
		delta = (t2 - t1) * 0.001;

		//inicjalizacja elementów
		context.renderer = renderer;
		context.charsetTexture = charsetTexture;
		context.characterTexture = characterTexture;
		context.ladderTexture = ladder_texture;
		context.waterTexture = water_texture;
		context.character = &character;
		context.currentStage = &currentStage;
		context.time = &gameStartTime;
		context.player = &player;
		context.gameRunning = &gameRunning;
		context.delta = delta;
		context.barrel = &barrel;
		context.barrelTexture = barrelTexture;
		
		t1 = t2;
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		switch (currentStage) {
		case 0:
			ShowMenu(renderer, charsetTexture, &player);
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT) {
					gameRunning = 0;
				}

				if (event.type == SDL_KEYDOWN) {
					switch (event.key.keysym.sym) {
					case SDLK_ESCAPE:
						gameRunning = 0;
						break;
					case SDLK_n:
						gameStartTime = SDL_GetTicks();
						InitCharacter(&character, characterTexture, 40, 420);
						break;
					case SDLK_RETURN:
						if (currentStage == 0) {
							currentStage = 1;
							gameStartTime = SDL_GetTicks();
							InitCharacter(&character, characterTexture, 40, 420);
						}
						break;
					case SDLK_1:
						currentStage = 1;
						gameStartTime = SDL_GetTicks();
						InitCharacter(&character, characterTexture, 40, 420);
						break;
					case SDLK_2:
						currentStage = 2;
						gameStartTime = SDL_GetTicks();
						InitCharacter(&character, characterTexture, 40, 420);
						break;
					case SDLK_3:
						currentStage = 3;
						gameStartTime = SDL_GetTicks();
						InitCharacter(&character, characterTexture, 40, 420);
						InitBarrel(&barrel, barrelTexture, 600, 100, 32, 32, -1, 1);
						break;
					case SDLK_BACKSPACE:
						// Usuwanie ostatniego znaku z nazwy gracza za pomoc¹ backspace
						int len = strlen(player.playerName);
						if (len > 0) {
							player.playerName[len - 1] = '\0';
						}
						break;
					}
				}
				if (event.type == SDL_TEXTINPUT && currentStage == 0) {
					// Dodawanie wprowadzanych znaków do nazwy gracza
					strcat(player.playerName, event.text.text);
				}
			}
			break;
		case 1:
			context.num_ladders = num_ladders;
			context.platforms = platforms;
			context.num_platforms = num_platforms;
			context.water = &water1;
			context.ladders = laddersInit;
			context.ladderplatform = ladders;
			level(context);
			break;
		case 2:
			context.num_ladders = num_ladders2;
			context.platforms = platforms2;
			context.num_platforms = num_platforms2;
			context.water = &water2;
			context.ladders = &laddersInit2;
			context.ladderplatform = ladders2;
			level(context);
			break;
		case 3:
			context.num_ladders = num_ladders3;
			context.platforms = platforms3;
			context.num_platforms = num_platforms3;
			context.water = &water1;
			context.ladders = &laddersInit2;
			context.ladderplatform = ladders2;
			level(context);
			break;
		default:
			break;
		}
		SDL_RenderPresent(renderer);
	}

	SDL_DestroyTexture(ladder_texture);
	SDL_DestroyTexture(characterTexture);
	SDL_DestroyTexture(charsetTexture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}