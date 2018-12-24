#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_framerate.h>

#include <cstdlib>
#include <cassert>
#include <ctime>
#include <sys/time.h>

static const float GLOBAL_FPS = 30.0;

#define MAX(x,y) (x < y ? x : y)
#define MIN(x,y) (x > y ? x : y)
#define CLAMP(min,max,val) MAX(min, MIN(val, max))

/**
 * UNIX epoch time, with milliseconds after the decimal point
 */
double
millitime(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec + (now.tv_usec / 1000000.0);
}

class Rect {
private:
    int m_x, m_y;
    int m_w, m_h;

public:
    Rect()
    : m_x(0), m_y(0), m_w(0), m_h(0)
    { }

    Rect(int x, int y, int w, int h)
    : m_x(x), m_y(y), m_w(w), m_h(h)
    { }    

    virtual ~Rect() {

    }

    int width(int w) {
        return m_w = w;
    }

    int width() {
        return m_w;
    }

    int height() {
        return m_h;
    }

    int height(int h) {
        return m_h = h;
    }

    int left() {
        return m_x;
    }

    int left(int x) {
        return m_x = x;
    }

    int right() {
        return m_x + m_w;
    }

    int top(int y) {
        return m_y = y;
    }

    int top() {
        return m_y;
    }

    int bottom() {
        return m_y + m_h;
    }

    int horizontalCenter() {
        return m_x + (m_w/2);
    }

    int verticalCenter() {
        return m_y + (m_h/2);
    }

    void moveTo(int x, int y) {
        m_x = x;
        m_y = y;
    }

    SDL_Rect getSDL_Rect() {
        // XXX: what happens when we go out of bounds/overflow?
        SDL_Rect ret;
        ret.x = left();
        ret.y = top();
        ret.w = width();
        ret.h = height();
        return ret;
    }

    bool collidesWith( Rect *B )
    {
        if( bottom() <= B->top() ) return false;
        if( top() >= B->bottom() ) return false;    
        if( right() <= B->left() ) return false;
        if( left() >= B->right() ) return false;        
        return true;
    }
};

class Sprite : public Rect {
public:
    Sprite(int width, int height)
    : Rect(0, 0, width, height), m_visible(true), m_surface(NULL)
    {
        assert(width > 0);
        assert(height > 0);
        m_surface = SDL_CreateRGBSurface(SDL_SWSURFACE|SDL_SRCALPHA, width, height,
                                         32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    }

    virtual ~Sprite() {
        SDL_FreeSurface(m_surface);
    }

    SDL_Surface *getSurface() {
        return m_surface;
    }

    void setVisible( bool visible ) {
        m_visible = visible;
    }

    bool isVisible() {
        return m_visible;
    }

    virtual void draw(SDL_Surface *screen, Rect *viewport) {
        if( isVisible() && collidesWith(viewport) ) {
            assert( viewport->width() == screen->w );
            assert( viewport->height() == screen->h );

            SDL_Rect dstrect;
            dstrect.x = this->left() - viewport->left();
            dstrect.y = this->top() - viewport->top();
            dstrect.w = this->width();
            dstrect.h = this->height();

            SDL_BlitSurface(m_surface, &m_surface->clip_rect, screen, &dstrect);
        }
    }   

protected:  
    bool m_visible;
    SDL_Surface *m_surface;
};


class Game_State {
public:
    Game_State()
    : m_event(NULL), m_now(0.0), m_score(0), m_coins(0)
    , m_chain_expire(0.0), m_chain_time(1.1), m_next_wave(0.0), m_wave(0)
    { }

    void think(SDL_Event *event) {        
        m_event = event;
        m_now = millitime();
        if( m_chain_expire < m_now ) {
            m_chain_expire = m_now;
        }

        if( m_next_wave <= m_now ) {
            m_next_wave = m_now + WAVE_DURATION;
            m_wave++;
        }
    }

    bool isCoinChained() const {
        return m_chain_expire > m_now;
    }

    bool collectCoin() {
        m_chain_expire += m_chain_time;
        m_score += 1 * coinMultiplier();

        return coinMultiplier() > 5;
    }

    double coinMultiplier() const {
        double m = (m_chain_expire - m_now) / 2;
        if( m < 1.0 ) return 1.0;
        if( m > 10.0 ) return 10.0;
        return m;
    }

    double now() const {
        return m_now;
    }

    int score() const {
        return m_score;
    }

    int coins() const {
        return m_coins;
    }

    double waveTimeRemaining() const {
        return m_next_wave - m_now;
    }

    double waveDuration() {
        return WAVE_DURATION;
    }

    double waveTimeSoFar() {
        return WAVE_DURATION - waveTimeRemaining();
    }

    int wave() const {
        return m_wave;
    }

    Rect* viewport() {
        return &m_viewport;
    }

    SDL_Event *event() const {
        return m_event;
    }

protected:
    SDL_Event *m_event;
    double m_now;
    int m_score;
    int m_coins;
    double m_chain_expire;
    double m_chain_time;
    Rect m_viewport;
    double m_next_wave;
    int m_wave;

    static const int WAVE_DURATION = 8;
};

class Game_Entity {
public:
    Game_Entity() {}
    virtual ~Game_Entity() {

    }
    virtual void reset(Game_State *state) = 0;
    virtual void think(Game_State *state) = 0;
};


class Cloud_Sprite : public Sprite, public Game_Entity {
public:
    Cloud_Sprite(int width, int height)
    : Sprite(width, height), m_sleep(0), m_velocity(0)
    {
        SDL_Surface *surface = getSurface();
        SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0xff, 0xff, 0xff, 0xC0));
    }

    int velocity() { 
        return m_velocity;
    }
    
    virtual void reset(Game_State *state) {
        Rect *viewport = state->viewport();
        int cloud_width = viewport->width()/5;
        int cloud_height = viewport->height()/8;
        int pos_x;

        setVisible(false);
        m_sleep = random() % 60;
        m_velocity = 1 + (random() % 6);
        if( random() % 2 ) {
            m_velocity = 0 - m_velocity;
        }

        if( m_velocity > 0 ) {
            pos_x = viewport->left() - (cloud_width - 2);
        }
        else {
            pos_x = viewport->left()  + (viewport->width() - 2);
        }

        int pos_y = (viewport->height()/2) + (random() % ((viewport->height()/2) - cloud_height));
        moveTo(pos_x, pos_y);
    }

    virtual void think(Game_State *state) {
        m_sleep--;
        if( m_sleep > 0 ) {
            return;
        }
        else if( m_sleep == 0 ) {
            setVisible(true);
        }

        moveTo(left() + m_velocity, top());
        if( ! collidesWith(state->viewport()) ) {
            reset(state);
        }    
    }

protected:
    int m_sleep;
    int m_velocity;
};

class Coin_Sprite : public Sprite, public Game_Entity {
public:
    Coin_Sprite(int size)
    : Sprite(size, size), Game_Entity(), m_target_x(10), m_target_y(10)
    {
        SDL_Surface *surface = getSurface();
        int half_w = surface->w / 2;
        int half_h = surface->h / 2;
        SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0xff, 0xff, 0xff, 0x00));
        filledEllipseRGBA(surface, half_w, half_h, half_w - 2 , half_h - 2, 0xFB, 0xB9, 0x17, 0xE0);
    }

    virtual void think(Game_State *state) {
        double tsf = state->waveTimeSoFar();
        Rect *viewport = state->viewport();
        
        double v = m_target_x+state->now() * 2.5;
        int pos_x, pos_y;
        if( m_target_x % 2 ) {            
            pos_x = m_target_x + ((width()/3.5) * sin(v+tsf));
            pos_y = m_target_y + ((width()/3.5) * cos(v));
        }
        else {
            pos_x = m_target_x + ((width()/3.5) * cos(v+tsf));
            pos_y = m_target_y + ((width()/3.5) * sin(v));
        }
        moveTo(pos_x, pos_y);
    }

    virtual void reset(Game_State *state) {
        Rect *viewport = state->viewport();
        m_target_x = viewport->left() + (random() % (viewport->width() - width()));
        m_target_y = viewport->top() + (random() % (viewport->height()/3*2));                
        setVisible(true);
    }
private:
    int m_target_x;
    int m_target_y;
};

class Diver_Sprite : public Sprite, public Game_Entity {
public:
    Diver_Sprite(int size)
    : Sprite(size, size), Game_Entity()
    , MOVE_RATE(1.2), m_velocity_x(0.0), m_velocity_y(-0.1)
    {
        SDL_Surface *surface = getSurface();
        SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0x00, 0x00, 0xff));
    }

    bool isFalling() {
        return m_velocity_y < 0.0;
    }

    void moveLeft() {
        m_velocity_x -= MOVE_RATE * 2.5;       
    }

    void moveRight() {
        m_velocity_x += MOVE_RATE * 2.5;
    }

    void bounceUp() {
        m_velocity_y = MOVE_RATE * 25;
    }

    void smallBounceUp() {
        m_velocity_y = MOVE_RATE * 15;
    }

    void bounceLeft() {
        m_velocity_x = 0 - (MOVE_RATE * 20.0);
    }

    void bounceRight() {
        m_velocity_x = (MOVE_RATE * 20.0);
    }

    virtual void reset(Game_State *state) {
        
    }

    virtual void think(Game_State *state) {
        SDL_Event *event = state->event();
        if( event && event->type == SDL_KEYDOWN ) {
            SDL_KeyboardEvent *key = &event->key;
            if( key->keysym.sym == SDLK_LEFT ) {
                moveLeft();
            }
            else if( key->keysym.sym == SDLK_RIGHT ) {
                moveRight();
            }            
        }

        Rect *viewport = state->viewport();
        m_velocity_x /= MOVE_RATE;
        m_velocity_y -= MOVE_RATE;
        
        moveTo(left() + m_velocity_x, top() - m_velocity_y);

        if( bottom() > viewport->bottom() ) {
            smallBounceUp();
        }

        if( ! collidesWith(viewport) ) {
            if( left() <= viewport->left() ) {
                bounceRight();
            }

            if( right() >= viewport->right() ) {
                bounceLeft();
            }
        }
    }

    /* Display 'position arrow' when Dan is off screen */
    virtual void draw(SDL_Surface *screen, Rect *viewport) {
        if( top() < viewport->top() ) {
            int width = screen->w / 40;
            int height = width / 2;
            int center = horizontalCenter();
            int left = center - (width / 2);
            int right = center + (width / 2);
            filledTrigonRGBA(screen, left, height, right, height, center, 0, 0xff, 0xff, 0xff, 0xc0);
        }

        Sprite::draw(screen, viewport);
    }

public:
    double MOVE_RATE;
    double m_velocity_x;
    double m_velocity_y;
};

class Scene {
protected:
    Scene(int width, int height)
    : m_width(width), m_height(height)
    {}

public:
    int width() {
        return m_width;
    }

    int height() {
        return m_height;
    }
    virtual ~Scene() {}
    virtual void think(SDL_Event *state) = 0;
    virtual void draw(SDL_Surface *screen) = 0;

private:
    int m_width;
    int m_height;
};

class Game_Scene : public Scene {
public:
    Game_Scene(int width, int height)
    : Scene(width, height), m_wave(-1)
    {
        Rect *viewport = m_state.viewport();
        viewport->left(0);
        viewport->top(0);
        viewport->width(width);
        viewport->height(height);

        int cloud_width = width/5;
        int cloud_height = height/8;
        size_t i;
        for( i = 0; i < CLOUD_COUNT; i++ ) {            
            m_clouds[i] = new Cloud_Sprite(cloud_width, cloud_height);
            m_clouds[i]->reset(&m_state);
        }

        int coin_size = width/25;
        for( i = 0; i < COIN_COUNT; i++ ) {
            m_coins[i] = new Coin_Sprite(coin_size);
            m_coins[i]->reset(&m_state);
        }

        int diver_width = width / 20;
        m_diver = new Diver_Sprite(diver_width);
        m_diver->moveTo(width/2-(diver_width/2), height/2-(diver_width/2));
    }

    virtual ~Game_Scene() {
        size_t i;

        delete m_diver;

        for( i = 0; i < CLOUD_COUNT; i++ ) {
            if( m_clouds[i] ) {
                delete m_clouds[i];
            }
        }

        for( i = 0; i < COIN_COUNT; i++ ) {
            if( m_coins[i] ) {
                delete m_coins[i];
            }
        }        
    }

    void moveViewport() {
        Rect *viewport = m_state.viewport();
        double viewport_distance = (m_diver->horizontalCenter() - viewport->horizontalCenter());
        int mod = (viewport_distance / 40.3);
        viewport->left( viewport->left() + mod );
    }

    virtual void think(SDL_Event *event) {    
        m_state.think(event);      
        m_diver->think(&m_state);  
        moveViewport();

        size_t i;
        bool cloud_collide = false;
        for( i = 0; i < CLOUD_COUNT; i++ ) {
            if( m_clouds[i] ) {
                if( ! cloud_collide && m_diver->isFalling() && m_clouds[i]->isVisible() && m_clouds[i]->collidesWith(m_diver) ) {
                    m_diver->bounceUp();                    
                    cloud_collide = true;
                }
                m_clouds[i]->think(&m_state);
            }
        }

        for( i = 0; i < COIN_COUNT; i++ ) {
            if( m_coins[i] ) {
                if( m_coins[i]->isVisible() && m_coins[i]->collidesWith(m_diver) ) {
                    m_coins[i]->reset(&m_state);
                    m_state.collectCoin();
                }
                m_coins[i]->think(&m_state);
            }
        }        
    }

    virtual void drawScore(SDL_Surface *screen) {
        char score_txt[30];
        sprintf(score_txt, "%d points", m_state.score());
        stringRGBA (screen, 10, 10, score_txt, 0, 0, 0, 0xff);

        bool show_multiplier;
        if( m_state.coinMultiplier() < 8.0 ) show_multiplier = true;
        else if( (int)((m_state.now() - (int)m_state.now()) * 10.0) % 2 ) show_multiplier = true;

        SDL_Rect multirect;
        if( show_multiplier ) {            
            multirect.x = 10;
            multirect.y = 25;
            multirect.h = 5;
            multirect.w = m_state.coinMultiplier() * 10;
            int red = (multirect.w / 100.0) * 0xff;
            int green = (1.0 - (multirect.w / 100.0)) * 0xff;
            SDL_FillRect(screen, &multirect, SDL_MapRGBA(screen->format, red, green, 0x00, 0xC0));
        }

        multirect.x = 10;
        multirect.y = 40;
        multirect.h = 5;
        multirect.w = (m_state.waveTimeRemaining() / m_state.waveDuration()) * 100;
        int red = (1.0 - (multirect.w / 100.0)) * 0xff;
        int blue = (multirect.w / 100.0) * 0xff;
        SDL_FillRect(screen, &multirect, SDL_MapRGBA(screen->format, red, 0x00, blue, 0xC0));
    }

    void drawBackground(SDL_Surface *screen) {
        size_t distance = screen->w / 15;
        size_t i = distance;        
        SDL_Rect box;
        box.y = 0;
        box.w = 5;
        box.h = screen->h;
        size_t start = m_state.viewport()->left() % distance;

        SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0x00, 0x56, 0xaf));        
        
        while( i-- ) {
            box.x = start;
            start += distance;
            SDL_FillRect(screen, &box, SDL_MapRGB(screen->format, 0x00, 0x56, 0xa0));
        }
    }

    virtual void draw(SDL_Surface *screen) {
        size_t i;        
        Rect *viewport = m_state.viewport();
        drawBackground(screen);
        for( i = 0; i < COIN_COUNT; i++ ) {
            if( m_coins[i] ) {
                m_coins[i]->draw(screen, viewport);
            }
        }
        for( i = 0; i < CLOUD_COUNT; i++ ) {
            if( m_clouds[i] ) {
                m_clouds[i]->draw(screen, viewport);
            }
        }        
        m_diver->draw(screen, viewport);        

        drawScore(screen);
    }

public:
    Game_State m_state;

    static const size_t CLOUD_COUNT = 4;
    Cloud_Sprite *m_clouds[CLOUD_COUNT];

    static const size_t COIN_COUNT = 10;
    Coin_Sprite *m_coins[COIN_COUNT];

    Diver_Sprite *m_diver;

    int m_wave;
};

class Intro_Scene : public Scene {
public:
    Intro_Scene(int width, int height)
    : Scene(width, height)
    , m_opacity(0.0), m_starttime(0.0), m_fadein(2.0)
    {
    }

    virtual ~Intro_Scene()
    { }

    virtual void think(SDL_Event *event) {
        double now = millitime();

        if( m_starttime == 0.0 ) {
            m_starttime = now;
        }

        // Fade-in over 5 seconds (m_fadein)
        m_time = now - m_starttime;
        if( m_time >= m_fadein ) {
            m_opacity = 1.0;
        }
        else {
            m_opacity = m_time / m_fadein;
        }

    }

    virtual void draw(SDL_Surface *screen) {
        int box_width, box_height;
        int origin_x, origin_y;     
        int rows, cols;

        /* 1-bit text: DERP */
        static const bool intro_logo[][16] = {
            {1,1,1,0,1,1,1,1,1,1,1,0,1,1,1,0},
            {1,0,0,1,1,0,0,0,1,0,0,1,1,0,0,1},
            {1,0,0,1,1,1,1,0,1,1,1,0,1,1,1,0},
            {1,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0},
            {1,1,1,0,1,1,1,1,1,0,0,1,1,0,0,0},
        };
        cols = sizeof(intro_logo[0]) / sizeof(intro_logo[0][0]);
        rows = sizeof(intro_logo) / sizeof(intro_logo[0]);

        box_height = box_width = screen->w / (cols*2);
        origin_x = (screen->w / 2) - ((cols * box_width) / 2);
        origin_y = (screen->h / 2) - ((rows * box_height) / 2);

        /* Oooo, sinewave sparkles! */
        int x, y;
        SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 10, 10, 10));
        for( x = 0; x < cols; x++ ) {
            for( y = 0; y < rows; y++ ) {
                bool show_box = intro_logo[y][x] > 0;
                if( show_box ) {
                    float tick = m_time * 2.0;
                    float color = ((255.0/2) * (cos(tick+(1+x)*(1+y)/20.2))) + (255.0/2);
                    float opacity2 = fabs(sin(x+tick/2.3+y)*cos(y+tick/2.3+x));
                    float size = fabs(sin(tick+(1+x)*(1+y)/20.2)) * opacity2;

                    int pos_x = origin_x + (x * box_width + (box_width/2));
                    int pos_y = origin_y + (y * box_width + (box_width/2));

                    /* And magic happens */
                    if( m_opacity < 1.0 ) {
                        pos_x += (sin(x+m_time)*cos(y) * screen->w) * (1.0 - m_opacity);
                        pos_y += (cos(y+m_time)*sin(x) * screen->h) * (1.0 - m_opacity);
                    }

                    int radius = (size * (box_width/4)) + (box_width/5);
                    filledCircleRGBA(screen, pos_x, pos_y, radius, color, 0xff-color, 0xff, 0xff*opacity2*m_opacity);
                }
            }
        }
    }

private:
    double m_time;
    double m_opacity;
    double m_starttime;
    double m_fadein;
};

class Intro2Game_Controller_Scene : public Scene {
public:
    Intro2Game_Controller_Scene(int width, int height)
    : Scene(width, height), m_intro(true), m_starttime(0.0), m_introend(0.0), m_subscene(NULL)
    {
        m_subscene = new Intro_Scene(width, height);
        m_starttime = millitime();
        m_introend = m_starttime + 5.0;
    }

    virtual void think(SDL_Event *event) {        
        if( m_intro ) {
            if( (event && event->type == SDL_KEYDOWN) || millitime() >= m_introend ) {
                delete m_subscene;
                m_subscene = new Game_Scene(width(), height());
                m_intro = false;
            }
        }
        m_subscene->think(event);
    }

    virtual void draw(SDL_Surface *screen) {
        m_subscene->draw(screen);
    }

private:
    bool m_intro;
    double m_starttime;
    double m_introend;
    Scene *m_subscene;
};

class Engine {
public:
    Engine(int width, int height)
    : m_scene(NULL), m_screen(NULL), m_quit(false)
    {
        SDL_Init( SDL_INIT_VIDEO );
        m_screen = SDL_SetVideoMode( width, height, 0, SDL_SWSURFACE|SDL_DOUBLEBUF );
        SDL_WM_SetCaption("Sky Dive Dan", 0);

        SDL_initFramerate(&m_fps);
        SDL_setFramerate(&m_fps, GLOBAL_FPS);

        SDL_EnableKeyRepeat(1000/30,1000/30);
    }

    void setScene( Scene *scene ) {
        m_scene = scene;
    }

    ~Engine() {
        SDL_Quit();
    }

    void run() {
        uint64_t tick = 0;
        while( ! m_quit ) {
            tick++;
            bool has_event = false;
            if( SDL_PollEvent(&m_event) ) {
                has_event = true;
                if( m_event.type == SDL_QUIT ) {
                    m_quit = true;
                }
            }

            m_scene->think(has_event ? &m_event : NULL);
            m_scene->draw(m_screen);

            SDL_Flip(m_screen); 
            SDL_framerateDelay(&m_fps);
        }
    }

private:
    Scene *m_scene;
    SDL_Surface *m_screen;
    SDL_Event m_event;
    FPSmanager m_fps;
    bool m_quit;
};

int main(int argc, char **argv) {
    srand(time(NULL));
    Engine game(800, 600);
    Intro2Game_Controller_Scene intro(800, 600);
    game.setScene(&intro);
    game.run();
    return 0; 
}