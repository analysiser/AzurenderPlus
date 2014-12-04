#include "RaytracerApplication.hpp"
namespace _462 {
    bool RaytracerApplication::initialize()
    {
        // copy camera into camera control so it can be moved via mouse
        camera_control.camera = scene.camera;
        bool load_gl = options.open_window;

        try {

            Material* const* materials = scene.get_materials();
            Mesh* const* meshes = scene.get_meshes();

            // load all textures
            for ( size_t i = 0; i < scene.num_materials(); ++i )
            {
                if (!materials[i]->load())
                {
                    std::cout << "Error loading texture, aborting.\n";
                    return false;
                }
                if (!materials[i]->create_gl_data())
                {
                    std::cout << "Error loading texture, aborting.\n";
                    return false;
                }
            }

            // load all meshes
            for ( size_t i = 0; i < scene.num_meshes(); ++i )
            {
                if ( !meshes[i]->load() )
                {
                    std::cout << "Error loading mesh, aborting.\n";
                    return false;
                }
                if (!meshes[i]->create_gl_data())
                {
                    std::cout << "Error loading mesh, aborting.\n";
                    return false;
                }
            }

        }
        catch ( std::bad_alloc const& )
        {
            std::cout << "Out of memory error while initializing scene\n.";
            return false;
        }

        // set the gl state
        if ( load_gl ) {
            float arr[4];
            arr[3] = 1.0; // alpha is always 1

            glClearColor(
                         scene.background_color.r,
                         scene.background_color.g,
                         scene.background_color.b,
                         1.0f );

            scene.ambient_light.to_array( arr );
            glLightModelfv( GL_LIGHT_MODEL_AMBIENT, arr );

           Light* const* lights = scene.get_lights();

            for ( size_t i = 0; i < NUM_GL_LIGHTS && i < scene.num_lights(); i++ )
            {
//                const SphereLight& light = lights[i];
//                PointLight *light = lights[i];
                glEnable( LightConstants[i] );
                lights[i]->color.to_array( arr );
                glLightfv( LightConstants[i], GL_DIFFUSE, arr );
                glLightfv( LightConstants[i], GL_SPECULAR, arr );
//                glLightf( LightConstants[i], GL_CONSTANT_ATTENUATION,
//                         light.attenuation.constant );
//                glLightf( LightConstants[i], GL_LINEAR_ATTENUATION,
//                         light.attenuation.linear );
//                glLightf( LightConstants[i], GL_QUADRATIC_ATTENUATION,
//                         light.attenuation.quadratic );
            }

            glLightModeli( GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE );
        }

        return true;
    }

    void RaytracerApplication::destroy()
    {

    }

    void RaytracerApplication::update( real_t delta_time )
    {
        if ( raytracing ) {
            // do part of the raytrace
            if ( !raytrace_finished ) {
                assert( buffer );
//                printf("delta time = %f\n",delta_time);
//                raytrace_finished = raytracer.raytrace( buffer, nullptr );
                raytrace_finished = raytracer.raytrace( buffer, &delta_time );
//                raytrace_finished = raytracer.PacketizedRayTrace( buffer );
            }

        } else {
            // copy camera over from camera control (if not raytracing)
            camera_control.update( delta_time );
            scene.camera = camera_control.camera;
        }
    }

    void RaytracerApplication::render()
    {
        int width, height;

        // query current window size, resize viewport
        get_dimension( &width, &height );
        glViewport( 0, 0, width, height );

        // fix camera aspect
        Camera& camera = scene.camera;
        camera.aspect = real_t( width ) / real_t( height );

        // clear buffer
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        // reset matrices
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity();
        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();

        if ( raytracing ) {
            // if raytracing, just display the buffer
            assert( buffer );
            glColor4d( 1.0, 1.0, 1.0, 1.0 );
            glRasterPos2f( -1.0f, -1.0f );
            glDrawPixels( buf_width, buf_height, GL_RGBA,
                         GL_UNSIGNED_BYTE, &buffer[0] );

        } else {
            // else, render the scene using opengl
            glPushAttrib( GL_ALL_ATTRIB_BITS );
            render_scene( scene , raytracer);
            glPopAttrib();
        }
    }

    void RaytracerApplication::handle_event( const SDL_Event& event )
    {
        int width, height;

        if ( !raytracing ) {
            camera_control.handle_event( this, event );
        }

        switch ( event.type )
        {
            case SDL_KEYDOWN:
                switch ( event.key.keysym.sym )
            {
                case KEY_RAYTRACE:
                    get_dimension( &width, &height );
                    toggle_raytracing( width, height );
                    std::cout<<"Camera Status"<<std::endl;
                    std::cout<<camera_control.camera.position<<std::endl;
                    std::cout<<camera_control.camera.orientation<<std::endl;
                    break;
                case KEY_SCREENSHOT:
                    output_image();
                    break;
                default:
                    break;
            }
            default:
                break;
        }
    }

    void RaytracerApplication::toggle_raytracing( int width, int height )
    {
        assert( width > 0 && height > 0 );

        // do setup if starting a new raytrace
        if ( !raytracing ) {

            // only re-allocate if the dimensions changed
            if ( buf_width != width || buf_height != height )
            {
                free( buffer );
                buffer = (unsigned char*) malloc( BUFFER_SIZE( width, height ) );
                if ( !buffer ) {
                    std::cout << "Unable to allocate buffer.\n";
                    return; // leave untoggled since we have no buffer.
                }
                buf_width = width;
                buf_height = height;
            }

            // initialize the raytracer (first make sure camera aspect is correct)
            scene.camera.aspect = real_t( width ) / real_t( height );

            if (!raytracer.initialize(&scene, options.num_samples, width, height))
            {
                std::cout << "Raytracer initialization failed.\n";
                return; // leave untoggled since initialization failed.
            }

            // reset flag that says we are done
            raytrace_finished = false;
        }
        else
        {
            // clean buffer
            memset((unsigned char *)buffer, 0, BUFFER_SIZE(width, height));
        }

        raytracing = !raytracing;
    }

    void RaytracerApplication::output_image()
    {
        static const size_t MAX_LEN = 256;
        const char* filename;
        char buf[MAX_LEN];

        if ( !buffer ) {
            std::cout << "No image to output.\n";
            return;
        }

        assert( buf_width > 0 && buf_height > 0 );

        filename = options.output_filename;

        // if we weren't given a file, use a default name
        if (!filename)
        {
            char add = this->scene.node_rank + '0';
            imageio_gen_name(buf, MAX_LEN);
            string tmp = "";
            tmp += add;
            tmp += buf;

            filename = tmp.c_str();
        }

        if (imageio_save_image(filename, buffer, buf_width, buf_height))
        {
            std::cout << "Saved raytraced image to '" << filename << "'.\n";
        } else {
            std::cout << "Error saving raytraced image to '" << filename << "'.\n";
        }
    }


    static void render_scene( const Scene& scene , Raytracer raytracer)
    {
        // backup state so it doesn't mess up raytrace image rendering
        glPushAttrib( GL_ALL_ATTRIB_BITS );
        glPushClientAttrib( GL_CLIENT_ALL_ATTRIB_BITS );

        glClearColor(
                     scene.background_color.r,
                     scene.background_color.g,
                     scene.background_color.b,
                     1.0f );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        glEnable( GL_NORMALIZE );
        glEnable( GL_DEPTH_TEST );
        glEnable( GL_LIGHTING );
        glEnable( GL_TEXTURE_2D );

        // set camera transform

        const Camera& camera = scene.camera;

        glMatrixMode( GL_PROJECTION );
        glLoadIdentity();

//#ifdef __APPLE__
//        GLKMatrix4MakePerspective( camera.get_fov_degrees(),
//                                  camera.get_aspect_ratio(),
//                                  camera.get_near_clip(),
//                                  camera.get_far_clip() );
//#else
        gluPerspective( camera.get_fov_degrees(),
                       camera.get_aspect_ratio(),
                       camera.get_near_clip(),
                       camera.get_far_clip() );
//#endif

        const Vector3& campos = camera.get_position();
        const Vector3 camref = camera.get_direction() + campos;
        const Vector3& camup = camera.get_up();

        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();

//#ifdef __APPLE__
//        GLKMatrix4MakeLookAt( campos.x, campos.y, campos.z,
//                            camref.x, camref.y, camref.z,
//                            camup.x,  camup.y,  camup.z );
//#else
        gluLookAt( campos.x, campos.y, campos.z,
                  camref.x, camref.y, camref.z,
                  camup.x,  camup.y,  camup.z );
//#endif
        // set light data
        float arr[4];
        arr[3] = 1.0; // w is always 1

        scene.ambient_light.to_array( arr );
        glLightModelfv( GL_LIGHT_MODEL_AMBIENT, arr );

        glLightModeli( GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE );

//        const SphereLight* lights = scene.get_lights();
        Light* const* lights = scene.get_lights();

        for ( size_t i = 0; i < NUM_GL_LIGHTS && i < scene.num_lights(); i++ )
        {
//            const SphereLight& light = lights[i];
            glEnable( LightConstants[i] );
            lights[i]->color.to_array( arr );
            glLightfv( LightConstants[i], GL_DIFFUSE, arr );
            glLightfv( LightConstants[i], GL_SPECULAR, arr );
//            glLightf( LightConstants[i], GL_CONSTANT_ATTENUATION,
//                     light.attenuation.constant );
//            glLightf( LightConstants[i], GL_LINEAR_ATTENUATION,
//                     light.attenuation.linear );
//            glLightf( LightConstants[i], GL_QUADRATIC_ATTENUATION,
//                     light.attenuation.quadratic );
            lights[i]->position.to_array( arr );
            glLightfv( LightConstants[i], GL_POSITION, arr );
        }
        // render each object

        Geometry* const* geometries = scene.get_geometries();

        for (size_t i = 0; i < scene.num_geometries(); ++i)
        {
            const Geometry& geom = *geometries[i];
            Vector3 axis;
            real_t angle;

            glPushMatrix();

            glTranslated(geom.position.x, geom.position.y, geom.position.z);
            geom.orientation.to_axis_angle(&axis, &angle);
            glRotated(angle*(180.0/PI), axis.x, axis.y, axis.z);
            glScaled(geom.scale.x, geom.scale.y, geom.scale.z);

            geom.render();

            glPopMatrix();
        }

        // Visualize photons
        if (raytracer.kdtree_photon_caustic_list.size() + raytracer.kdtree_photon_indirect_list.size() > 0) {

            glDisable( GL_LIGHTING );
            glDisable( GL_DEPTH_TEST );
            glPushMatrix();
            glBegin(GL_POINTS);

            for (size_t i = 0; i < raytracer.kdtree_photon_caustic_list.size(); i++) {

                Color3 photonColor = raytracer.kdtree_photon_caustic_list[i].getColor();
                glColor3f(photonColor.r, photonColor.g, photonColor.b);
                glVertex3f(raytracer.kdtree_photon_caustic_list[i].position.x,
                           raytracer.kdtree_photon_caustic_list[i].position.y,
                           raytracer.kdtree_photon_caustic_list[i].position.z);
            }

            for (size_t i = 0; i < raytracer.kdtree_photon_indirect_list.size(); i++) {

                Color3 photonColor = raytracer.kdtree_photon_indirect_list[i].getColor();
                glColor3f(photonColor.r, photonColor.g, photonColor.b);
                glVertex3f(raytracer.kdtree_photon_indirect_list[i].position.x,
                           raytracer.kdtree_photon_indirect_list[i].position.y,
                           raytracer.kdtree_photon_indirect_list[i].position.z);
            }

            glEnd();
            glEnable( GL_LIGHTING );
            glEnable( GL_DEPTH_TEST );
            glPopMatrix();
        }

        glPopClientAttrib();
        glPopAttrib();
    }
}  // namespace _462
