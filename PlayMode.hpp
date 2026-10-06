#pragma once
#include "Mode.hpp"
#include <array>
#include <vector>

struct PlayMode : Mode {
    PlayMode();
    bool handle_event(SDL_Event const &, glm::uvec2 const &) override;
    void update(float elapsed) override;
    void draw(glm::uvec2 const &drawable_size) override;

    struct Body {
        glm::vec2 position{0.0f}, velocity{0.0f};
        float radius = 0.35f;
        float inverse_mass = 0.25f;
    };
    struct Shot { Body body; int owner; float life = 7.0f; };
    struct Player {
        glm::vec2 position;
        float angle = 0.0f;
        int ammo = 5;
        bool aim_up = false, aim_down = false, fire = false;
    };
    std::array<Player, 2> players;
    Body bomb;
    std::vector<Shot> shots;
    double accumulator = 0.0;
    int ticks_left = 2400; //20 seconds at 120 Hz.
    int loser = -1; //-1: playing, 0/1: losing player, 2: draw.

    void reset();
    void step(float dt);
    glm::vec2 aim(int player) const;
    static void bounce(Body &body);
    static void collide(Body &a, Body &b);
};
