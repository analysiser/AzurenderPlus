/**
 * @file main_rayracer.cpp
 * @brief Raytracer entry
 *
 * @author Eric Butler (edbutler)
 */


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

#include "RaytracerApplication.hpp"

using namespace _462;

/**
 * Prints program usage.
 */
static void print_usage( const char* progname )
{
    std::cout << "Usage: " << progname <<
	"input_scene [-n num_samples] [-r] [-d width"
	" height] [-o output_file]\n"
    "\n" \
    "Options:\n" \
    "\n" \
    "\t-r:\n" \
    "\t\tRaytraces the scene and saves to the output file without\n" \
    "\t\tloading a window or creating an opengl context.\n" \
    "\t-d width height\n" \
    "\t\tThe dimensions of image to raytrace (and window if using\n" \
    "\t\tand opengl context. Defaults to width=800, height=600.\n" \
    "\t-s input_scene:\n" \
    "\t\tThe scene file to load and raytrace.\n" \
    "\toutput_file:\n" \
    "\t\tThe output file in which to write the rendered images.\n" \
    "\t\tIf not specified, default timestamped filenames are used.\n" \
    "\n" \
    "Instructions:\n" \
    "\n" \
    "\tPress 'r' to raytrace the scene. Press 'r' again to go back to\n" \
    "\tgo back to OpenGL rendering. Press 'f' to dump the most recently\n" \
    "\traytraced image to the output file.\n" \
    "\n" \
    "\tUse the mouse and 'w', 'a', 's', 'd', 'q', and 'e' to move the\n" \
    "\tcamera around. The keys translate the camera, and left and right\n" \
    "\tmouse buttons rotate the camera.\n" \
    "\n" \
    "\tIf not using windowed mode (i.e., -r was specified), then output\n" \
    "\timage will be automatically generated and the program will exit.\n" \
    "\n";
}


/**
 * Parses args into an Options struct. Returns true on success, false on failure.
 */
static bool parse_args( Options* opt, int argc, char* argv[] )
{
    if ( argc < 2 ) {
        print_usage( argv[0] );
        return false;
    }

	opt->input_filename = argv[1];
	opt->output_filename = NULL;
	opt->open_window = true;
	opt->width = DEFAULT_WIDTH;
	opt->height = DEFAULT_HEIGHT;
	opt->num_samples = 1;
	for (int i = 2; i < argc; i++)
	{
		switch (argv[i][1])
		{
            case 'd':
                if (i >= argc - 2) return false;
                opt->width = atoi(argv[++i]);
                opt->height = atoi(argv[++i]);
                // check for valid width/height
                if ( opt->width < 1 || opt->height < 1 )
                {
                    std::cout << "Invalid window dimensions\n";
                    return false;
                }
                break;
            case 'r':
                opt->open_window = false;
                break;
            case 'n':
                if (i < argc - 1)
                    opt->num_samples = atoi(argv[++i]);
                break;
            case 'o':
                if (i < argc - 1)
                    opt->output_filename = argv[++i];
		}
	}

    return true;
}

int main(int argc, char* argv[])
{
    Options opt;

    Matrix3 mat;
    Matrix4 trn;
    make_transformation_matrix( &trn, Vector3::Zero(),
                               Quaternion::Identity(), Vector3( 2, 2, 2 ) );

    make_normal_matrix( &mat, trn );

    // MPI sample code
    // Initialize the MPI environment
    MPI_Init(NULL, NULL);

    // Get the number of processes.
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, & world_size);

    // Get the rank of the process.
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, & world_rank);

    // Get the name of the processor.
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, & name_len);

    // Print off a hello world message.
    printf("Hello world from processor %s, rank %d"
           " out of %d processors\n",
           processor_name, world_rank, world_size);

    if ( !parse_args( &opt, argc, argv ) ) {
        return 1;
    }

    RaytracerApplication app( opt );

    if (world_size > 1)
    {
        // Initialize the scene with MPI info
        app.scene.node_size = world_size;
        app.scene.node_rank = world_rank;
    }

    // load the given scene
    if ( !load_scene( &app.scene, opt.input_filename ) ) {
        std::cout << "Error loading scene "
        << opt.input_filename << ". Aborting.\n";
        return 1;
    }

    // either launch a window or do a full raytrace without one,
    // depending on the option
    if ( opt.open_window ) {

        real_t fps = 30.0;
        const char* title = "Azurender";
        // start a new application
        return Application::start_application(&app,
                                              opt.width,
                                              opt.height,
                                              fps, title);

    }
    else
    {
        app.initialize();
        app.toggle_raytracing( opt.width, opt.height );
        if ( !app.raytracing ) {
            return 1; // some error occurred
        }
        assert( app.buffer );
        // raytrace until done
        app.raytracer.raytrace( app.buffer, 0);

        MPI_Barrier(MPI_COMM_WORLD);

        app.gather_mpi_results(world_rank);

        MPI_Barrier(MPI_COMM_WORLD);

        // output result
        app.output_image();
        // Test for finalize
        MPI_Finalize();
        return 0;
    }
}
