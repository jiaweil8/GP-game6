#include "PlayMode.hpp"
#include "DrawLines.hpp"
#include "data_path.hpp"
#include "GL.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {
constexpr float Width = 8.0f, Height = 4.0f;
constexpr double Tick = 1.0 / 120.0;
constexpr float Pi = 3.14159265359f;
const glm::u8vec4 Cyan(80, 220, 255, 255), Pink(255, 110, 185, 255);
const glm::u8vec4 White(220, 235, 255, 255), Gold(255, 205, 75, 255);
}

PlayMode::PlayMode() : text_renderer(data_path("PaytoneOne-Regular.ttf")) { reset(); }

PlayMode::~PlayMode() {
    for (auto &entry : hud_text) text_renderer.destroy_text(entry.texture);
}

void PlayMode::reset() {
    players = {};
    players[0].position = glm::vec2(-7.0f, 0.0f);
    players[1].position = glm::vec2( 7.0f, 0.0f);
    bomb = Body{};
    bomb.velocity = glm::vec2(0.0f, 1.1f);
    shots.clear();
    accumulator = 0.0;
    ticks_left = 2400;
    loser = -1;
}

glm::vec2 PlayMode::aim(int player) const {
    float a = players[player].angle;
    return glm::vec2((player == 0 ? 1.0f : -1.0f) * std::cos(a), std::sin(a));
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &) {
    if (evt.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        for (auto &p : players) p.aim_up = p.aim_down = p.fire = false;
        return true;
    }
    if (evt.type != SDL_EVENT_KEY_DOWN && evt.type != SDL_EVENT_KEY_UP) return false;
    bool pressed = evt.type == SDL_EVENT_KEY_DOWN;
    if (pressed && evt.key.repeat) return true;
    switch (evt.key.key) {
    case SDLK_W: players[0].aim_up = pressed; break;
    case SDLK_S: players[0].aim_down = pressed; break;
    case SDLK_UP: players[1].aim_up = pressed; break;
    case SDLK_DOWN: players[1].aim_down = pressed; break;
    case SDLK_D: if (pressed) players[0].fire = true; break;
    case SDLK_LEFT: if (pressed) players[1].fire = true; break;
    case SDLK_R: if (pressed) reset(); break;
    default: return false;
    }
    return true;
}

void PlayMode::bounce(Body &b) {
    for (int axis = 0; axis < 2; ++axis) {
        float edge = (axis == 0 ? Width : Height) - b.radius;
        if (b.position[axis] < -edge) {
            b.position[axis] = -edge;
            b.velocity[axis] = std::abs(b.velocity[axis]);
        } else if (b.position[axis] > edge) {
            b.position[axis] = edge;
            b.velocity[axis] = -std::abs(b.velocity[axis]);
        }
    }
}

void PlayMode::collide(Body &a, Body &b) {
    glm::vec2 offset = b.position - a.position;
    float distance2 = glm::dot(offset, offset);
    float radius = a.radius + b.radius;
    if (distance2 >= radius * radius) return;
    float distance = std::sqrt(distance2);
    glm::vec2 normal = distance > 0.00001f ? offset / distance : glm::vec2(1.0f, 0.0f);
    float total_mass = a.mass + b.mass;
    //Move the lighter body more; only apply impulse when approaching.
    glm::vec2 correction = normal * (radius - distance);
    a.position -= correction * (b.mass / total_mass);
    b.position += correction * (a.mass / total_mass);
    float closing_speed = glm::dot(b.velocity - a.velocity, normal);
    if (closing_speed >= 0.0f) return;
    glm::vec2 impulse = -(1.0f + 0.9f) * closing_speed / (1.0f / a.mass + 1.0f / b.mass) * normal;
    a.velocity -= impulse / a.mass;
    b.velocity += impulse / b.mass;
}

void PlayMode::step(float dt) {
    for (int i = 0; i < 2; ++i) {
        auto &p = players[i];
        p.angle = std::clamp(p.angle + (float(p.aim_up) - float(p.aim_down)) * 1.3f * dt, -1.2f, 1.2f);
        if (p.fire && p.ammo > 0) {
            Body body;
            body.radius = 0.15f;
            body.mass = 1.0f;
            body.position = p.position + aim(i) * 0.65f;
            body.velocity = aim(i) * 4.5f;
            shots.push_back({body, i, 7.0f});
            --p.ammo;
        }
        p.fire = false;
    }
    //Substeps keep fast projectiles from skipping over the bomb.
    for (int sub = 0; sub < 4; ++sub) {
        bomb.position += bomb.velocity * (dt / 4.0f);
        bounce(bomb);
        for (auto &shot : shots) {
            shot.body.position += shot.body.velocity * (dt / 4.0f);
            bounce(shot.body);
            collide(bomb, shot.body);
        }
        for (size_t i = 0; i < shots.size(); ++i)
            for (size_t j = i + 1; j < shots.size(); ++j)
                collide(shots[i].body, shots[j].body);
        bounce(bomb);
        for (auto &shot : shots) bounce(shot.body);
    }
    for (auto &shot : shots) shot.life -= dt;
    shots.erase(std::remove_if(shots.begin(), shots.end(), [](Shot const &s) { return s.life <= 0.0f; }), shots.end());
    if (--ticks_left == 0) {
        float d0 = glm::length(bomb.position - players[0].position);
        float d1 = glm::length(bomb.position - players[1].position);
        loser = std::abs(d0 - d1) < 0.001f ? 2 : (d0 < d1 ? 0 : 1);
    }
}

void PlayMode::update(float elapsed) {
    if (loser != -1) return;
    accumulator += std::clamp(double(elapsed), 0.0, 0.1);
    while (accumulator >= Tick && loser == -1) {
        step(float(Tick));
        accumulator -= Tick;
    }
}

void PlayMode::draw(glm::uvec2 const &size) {
    if (size.x == 0 || size.y == 0) return;
    glClearColor(0.008f, 0.012f, 0.035f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    float aspect = float(size.x) / float(size.y);
    float half_h = std::max(6.2f, 8.7f / aspect);
    struct TextDraw { TextTexture texture; glm::vec2 position; float scale; glm::vec3 color; };
    std::vector<TextDraw> text_draws;
    size_t text_index = 0;
    {
    DrawLines lines(glm::ortho(-half_h * aspect, half_h * aspect, -half_h, half_h));
    auto line = [&](glm::vec2 a, glm::vec2 b, glm::u8vec4 c) { lines.draw(glm::vec3(a, 0.0f), glm::vec3(b, 0.0f), c); };
    auto circle = [&](glm::vec2 p, float r, glm::u8vec4 c) {
        for (int i = 0; i < 40; ++i) {
            float a = 2.0f * Pi * float(i) / 40.0f, b = 2.0f * Pi * float(i + 1) / 40.0f;
            line(p + r * glm::vec2(std::cos(a), std::sin(a)), p + r * glm::vec2(std::cos(b), std::sin(b)), c);
        }
    };
    auto text = [&](std::string const &s, float x, float y, float h, glm::u8vec4 c, bool centered = false) {
        auto &entry = hud_text[text_index++];
        if (entry.value != s) {
            text_renderer.destroy_text(entry.texture);
            entry.texture = text_renderer.make_text(s);
            entry.value = s;
        }
        float pixels_per_unit = float(size.y) / (2.0f * half_h);
        float scale = h * pixels_per_unit / 48.0f;
        glm::vec2 position(float(size.x) * 0.5f + x * pixels_per_unit,
            float(size.y) * 0.5f - y * pixels_per_unit - float(entry.texture.height) * scale);
        if (centered) position.x -= float(entry.texture.width) * scale * 0.5f;
        text_draws.push_back({entry.texture, position, scale, glm::vec3(c) / 255.0f});
    };
    for (int i = 0; i < 100; ++i) {
        glm::vec2 p(-7.9f + float((i * 137) % 997) / 997.0f * 15.8f, -3.9f + float((i * 293) % 991) / 991.0f * 7.8f);
        line(p, p + glm::vec2(0.025f, 0.025f), glm::u8vec4(75, 90, 125, 255));
    }
    line({-Width, -Height}, {Width, -Height}, White);
    line({Width, -Height}, {Width, Height}, White);
    line({Width, Height}, {-Width, Height}, White);
    line({-Width, Height}, {-Width, -Height}, White);
    for (int i = 0; i < 2; ++i) {
        auto const &p = players[i];
        auto color = i == 0 ? Cyan : Pink;
        circle(p.position, 0.4f, color);
        line(p.position, p.position + aim(i) * 0.9f, color);
        for (int a = 0; a < p.ammo; ++a) circle({p.position.x + (float(a) - 2.0f) * 0.23f, -0.85f}, 0.065f, color);
    }
    for (auto const &shot : shots) circle(shot.body.position, shot.body.radius, shot.owner == 0 ? Cyan : Pink);
    circle(bomb.position, bomb.radius, Gold);
    circle(bomb.position, bomb.radius * 0.65f, Gold);
    line(bomb.position, bomb.position + bomb.velocity * 0.18f, Gold);
    auto centered_text = [&](std::string const &s, float center_x, float y, float h, glm::u8vec4 color) {
        text(s, center_x, y, h, color, true);
    };
    centered_text("SPACE HOT POTATO", 0.0f, 5.65f, 0.30f, White);
    int seconds_left = (ticks_left + 119) / 120;
    const glm::u8vec4 Red(255, 70, 70, 255);
    centered_text("TIME " + std::to_string(seconds_left), 0.0f, 4.85f, 0.60f,
        seconds_left <= 5 ? Red : Gold);

    float distance_p1 = glm::length(bomb.position - players[0].position);
    float distance_p2 = glm::length(bomb.position - players[1].position);
    bool equal_distance = std::abs(distance_p1 - distance_p2) < 0.001f;
    bool p1_closer = distance_p1 < distance_p2;
    centered_text(equal_distance ? "EQUAL DISTANCE" : p1_closer ? "P1 IN DANGER" : "P2 IN DANGER",
        0.0f, 4.25f, 0.25f, equal_distance ? White : p1_closer ? Cyan : Pink);
    char distance_text[32];
    std::snprintf(distance_text, sizeof(distance_text), "P1 DIST %.2f", double(distance_p1));
    centered_text(distance_text, -5.7f, 4.35f, 0.28f, Cyan);
    std::snprintf(distance_text, sizeof(distance_text), "P2 DIST %.2f", double(distance_p2));
    centered_text(distance_text, 5.7f, 4.35f, 0.28f, Pink);
    text("P1: W/S AIM  D FIRE", -7.8f, -4.6f, 0.23f, Cyan);
    text("P2: UP/DOWN AIM  LEFT FIRE", 0.5f, -4.6f, 0.23f, Pink);
    text("5 SHOTS EACH   |   CLOSER TO EXPLOSION LOSES   |   R RESTART", -7.8f, -5.05f, 0.20f, White);
    if (loser != -1) {
        for (int i = 0; i < 16; ++i) {
            float a = 2.0f * Pi * float(i) / 16.0f;
            glm::vec2 direction(std::cos(a), std::sin(a));
            line(bomb.position + direction * 0.45f, bomb.position + direction * 1.1f, Gold);
        }
        centered_text(loser == 2 ? "DRAW!  R TO RESTART" : loser == 0 ? "P2 WINS!  R TO RESTART" : "P1 WINS!  R TO RESTART", 0.0f, 3.25f, 0.35f, White);
    }
    } //Draw arena lines before overlaying the text.
    for (auto const &entry : text_draws) {
        text_renderer.draw_text(entry.texture, entry.position, entry.scale, entry.color, size);
    }
}