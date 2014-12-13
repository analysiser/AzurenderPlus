//
//  Utils.h
//  P3
//
//  Created by Xiao Li on 5/3/14.
//
//

//#include <stdio.h>

#ifndef AZ_UTILS_H
#define AZ_UTILS_H

#define BUFFER_SIZE(w,h) ( (size_t) ( 4 * (w) * (h) ) )

struct cPhoton
{
	/* data */
    float position[3];              // 12
    unsigned int index;             // 4   // index in photon list
    int splitAxis;                  // 4     // char and short cannot be compiled by ispc
    int padding;
};

struct metaCPhoton
{
    unsigned int cphotonIndex;      // index in cphoton list
    unsigned int sortedIndex[3];    // index in sorted index lists
};

struct KDNode
{
    int cphotonIndex;
    int splitAxis;
    float splitValue;
    int head;           // including
    int tail;           // excluding
    int size;
    bool isLeaf;
    
    KDNode *left;
    KDNode *right;
};

/*!
 * Struct of framebuffer
 */
struct FrameBuffer
{
    unsigned char *cbuffer; // Color buffer rgba
    real_t *zbuffer;         // Depth buffer
    char *shadowMap;        // Shadow map
    
    void init()
    {
        cbuffer = nullptr;
        zbuffer = nullptr;
        shadowMap = nullptr;
    }
    
    void alloc(int width, int height)
    {
        assert(width > 0 && height > 0);
        cbuffer = (unsigned char *)malloc(BUFFER_SIZE(width, height));
        zbuffer = new real_t[width * height];
        shadowMap = new char[width * height];
    }
    
    void dealloc()
    {
        if (cbuffer)
        {
            delete cbuffer;
        }
        if (zbuffer)
        {
            delete zbuffer;
        }
        if (shadowMap)
        {
            delete shadowMap;
        }
    }
    
    bool isready()
    {
        return (cbuffer && zbuffer && shadowMap);
    }
    
    void cleanbuffer(int width, int height)
    {
        std::fill_n((unsigned char *)cbuffer, BUFFER_SIZE(width, height), 0);
        std::fill_n((real_t *)zbuffer, width * height, DBL_MAX);
        std::fill_n((char *)shadowMap, width * height, 0);
    }
};

#endif