#pragma once
#include "application/application.hpp"
#include "application/camera_roam.hpp"
#include "application/imageio.hpp"
#include "application/scene_loader.hpp"
#include "application/opengl.hpp"
#include "scene/scene.hpp"
#include "raytracer/raytracer.hpp"

#ifdef __APPLE__
#include <GLKit/GLKMath.h>
#endif

#include <SDL.h>

#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <vector>
#include "scene/ray.hpp"

#include <mpi.h>

namespace _462 {

#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGHT 600

#define BUFFER_SIZE(w,h) ( (size_t) ( 4 * (w) * (h) ) )

#define KEY_RAYTRACE SDLK_r
#define KEY_SCREENSHOT SDLK_f




    // pretty sure these are sequential, but use an array just in case
    static const GLenum LightConstants[] = {
        GL_LIGHT0, GL_LIGHT1, GL_LIGHT2, GL_LIGHT3,
        GL_LIGHT4, GL_LIGHT5, GL_LIGHT6, GL_LIGHT7
    };
    static const size_t NUM_GL_LIGHTS = 8;

    // renders a scene using opengl
    static void render_scene( const Scene& scene , Raytracer raytracer);

    /**
     * Struct of the program options.
     */
    struct Options
    {
        // whether to open a window or just render without one
        bool open_window;
        // not allocated, pointed it to something static
        const char* input_filename;
        // not allocated, pointed it to something static
        const char* output_filename;
        // window dimensions
        int width, height;
        int num_samples;
    };
    class RaytracerApplication : public Application
    {
    public:

        RaytracerApplication( const Options& opt )
        : options( opt ), buffer( 0 ), buf_width( 0 ),
        buf_height( 0 ), raytracing( false ) { }
        virtual ~RaytracerApplication() { free( buffer ); }

        virtual bool initialize();
        virtual void destroy();
        virtual void update( real_t );
        virtual void render();
        virtual void handle_event( const SDL_Event& event );

        // flips raytracing, does any necessary initialization
        void toggle_raytracing( int width, int height );
        // writes the current raytrace buffer to the output file
        void output_image();

        Raytracer raytracer;

        // the scene to render
        Scene scene;

        // options
        Options options;

        // the camera
        CameraRoamControl camera_control;

        // the image buffer for raytracing
        unsigned char* buffer;
        // width and height of the buffer
        int buf_width, buf_height;
        // true if we are in raytrace mode.
        // if so, we raytrace and display the raytrace.
        // if false, we use normal gl rendering
        bool raytracing;
        // false if there is more raytracing to do
        bool raytrace_finished;
    };

}  //namespace _462
