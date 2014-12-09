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
    float *zbuffer;         // Z buffer
    
    void init()
    {
        cbuffer = nullptr;
        zbuffer = nullptr;
    }
    
    void alloc(int width, int height)
    {
        assert(width > 0 && height > 0);
        cbuffer = (unsigned char *)malloc(BUFFER_SIZE(width, height));
        zbuffer = (float *)malloc(width * height * sizeof(float));
    }
    
    void dealloc()
    {
        if (cbuffer)
        {
            free(cbuffer);
        }
        if (zbuffer)
        {
            free(zbuffer);
        }
    }
    
    bool isready()
    {
        return (cbuffer && zbuffer);
    }
    
    void cleanbuffer(int width, int height)
    {
        memset((unsigned char *)cbuffer, 0, BUFFER_SIZE(width, height));
        memset((float *)zbuffer, FLT_MAX, width * height * sizeof(float));
    }
};

#endif