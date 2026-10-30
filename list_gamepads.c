#include <stdio.h>
#include <SDL3/SDL.h>

int main() {
    if (!SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    int num_joysticks = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&num_joysticks);
    printf("Number of connected joysticks/gamepads: %d\n", num_joysticks);
    for (int i = 0; i < num_joysticks; i++) {
        SDL_JoystickID jid = joysticks[i];
        const char* name = SDL_GetJoystickNameForID(jid);
        const char* path = SDL_GetJoystickPathForID(jid);
        printf("[%d] ID=%u Name='%s' Path='%s' IsGamepad=%d\n",
               i, (unsigned)jid, name ? name : "NULL", path ? path : "NULL",
               SDL_IsGamepad(jid));
    }
    SDL_free(joysticks);
    SDL_Quit();
    return 0;
}
