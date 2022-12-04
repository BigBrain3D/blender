#include "dualcon.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MVert {
  float co[3];
  /**
   * Deprecated flag for storing hide status and selection, which are now stored in separate
   * generic attributes. Kept for file read and write.
   */
  char flag_legacy;
  /**
   * Deprecated bevel weight storage, now located in #CD_BWEIGHT, except for file read and write.
   */
  char bweight_legacy;
  char _pad[2];
} MVert;

typedef struct {
  MVert *verts;
  unsigned *loops;
  int curvert, curface;
} DualConOutput2;


#define MEM_mallocN(size, str) ((void)str, malloc(size))
#define MEM_callocN(size, str) ((void)str, calloc(size, 1))
#define MEM_freeN(ptr) free(ptr)

void copy_v3_v3(void* a, void* b) {
  ((float*)a)[0] = ((float*)b)[0];
  ((float*)a)[1] = ((float*)b)[1];
  ((float*)a)[2] = ((float*)b)[2];
}

/* allocate and initialize a DualConOutput */
static void *dualcon_alloc_output2(int totvert, int totquad)
{
  DualConOutput2 *output;

  if (!(output = MEM_callocN(sizeof(DualConOutput2), "DualConOutput"))) {
    return NULL;
  }

  output->loops = MEM_callocN(4 * sizeof(unsigned) * totquad, "Custom Quads");

  output->verts = MEM_callocN(sizeof(MVert) * totvert, "Custom Verts");

  output->curvert = 0;
  output->curface = 0;

  return output;
}

static void dualcon_add_vert2(void *output_v, const float co[3])
{
  DualConOutput2 *output = output_v;

  copy_v3_v3(output->verts[output->curvert].co, co);

  output->curvert++;
}

static void dualcon_add_quad2(void *output_v, const int vert_indices[4])
{
  DualConOutput2 *output = output_v;

  unsigned *mloop = output->loops;

  for (int i = 0; i < 4; i++) {
    mloop[output->curface * 4 + i] = vert_indices[i];
  }

  output->curface++;
}

void load_obj(const char* filename, float **verticesPtr, int** loopPtr, int* vcount, int* tcount)
{
    FILE* in = fopen(filename, "rb");

    int vc = 0;
    int fc = 0;

    char* line = MEM_callocN(1024, "File read buffer");

    int sz = 0;

    while (fgets(line,1024,in))
    {
      if (line[0] == 'v') {
        MVert vert;
        sscanf(line, "v %f %f %f\n", &vert.co[0], &vert.co[1], &vert.co[2]);

        vc++;
      } else if (line[0] == 'f') {
        unsigned tri_loops[3];

        sscanf(line, "f %d %d %d\n", &tri_loops[0], &tri_loops[1], &tri_loops[2]);

        tri_loops[0]--;
        tri_loops[1]--;
        tri_loops[2]--;

        fc++;
      }
    }

    *tcount = fc;
    *vcount = vc;

    float* vertexbuffer = malloc(sizeof(float) * 3 * vc);

    int* indexbuffer = malloc(sizeof(int) * 3 * fc);

    vc = 0;

    fc = 0;

    fseek(in, 0, SEEK_SET);

    while (fgets(line,1024,in))
    {
      if (line[0] == 'v') {
        MVert vert;
        sscanf(line, "v %f %f %f", &vert.co[0], &vert.co[1], &vert.co[2]);

        vertexbuffer[3 * vc + 0] = vert.co[0];
        vertexbuffer[3 * vc + 1] = vert.co[1];
        vertexbuffer[3 * vc + 2] = vert.co[2];

        vc++;
      } else if (line[0] == 'f') {
        int tri_loops[3];

        sscanf(line, "f %d %d %d", &tri_loops[0], &tri_loops[1], &tri_loops[2]);

        tri_loops[0]--;
        tri_loops[1]--;
        tri_loops[2]--;

        indexbuffer[3 * fc + 0] = tri_loops[0];
        indexbuffer[3 * fc + 1] = tri_loops[1];
        indexbuffer[3 * fc + 2] = tri_loops[2];

        fc++;
      }
    }

    MEM_freeN(line);

    fclose(in);

    *verticesPtr = vertexbuffer;
    *loopPtr = indexbuffer;
}


#define INIT_MINMAX(min, max) \
  { \
    (min)[0] = (min)[1] = (min)[2] = 1.0e30f; \
    (max)[0] = (max)[1] = (max)[2] = -1.0e30f; \
  } \
  (void)0

int main(int argc, char* argv[]) {
    if (argc != 7) fprintf(stderr, "Wrong number of args.\nUsage: dconwrapper <.OBJ path> <threshold> <scale> <hermite> <Octree Depth> <Output .OBJ>");

    const char* obj_path = argv[1];

    float threshold = atof(argv[2]);

    float scale = atof(argv[3]);

    float hermit_num = atof(argv[4]);

    int octree_depth = atoll(argv[5]);

    const char* path = argv[6];
    
    int* indexdata;
    float* vertices;

    int tricount; 
    int vertcount;

    load_obj(obj_path, &vertices, &indexdata, &vertcount, &tricount);

    unsigned* loopdata = MEM_callocN(sizeof(unsigned) * vertcount, "Custom Loop Data");

    for (int i = 0; i < vertcount; i++) {
      loopdata[i] = i;
    }

    printf("%d vertices %d faces\n", vertcount, tricount);

    DualConInput input2;

    memset(&input2, 0, sizeof(DualConInput));

    input2.mloop = loopdata;

    input2.co = vertices;
    input2.co_stride = sizeof(float) * 3;
    input2.totco = vertcount;

    input2.looptri = indexdata;
    input2.tri_stride = sizeof(int) * 3;
    input2.tottri = tricount;

    input2.loop_stride = sizeof(int);

    float min[3];
    float max[3];

    INIT_MINMAX(min, max);

    for (int i = 0; i < vertcount; i++) {
      if (vertices[3 * i + 0] < min[0]) min[0] = vertices[3 * i + 0];
      else if (vertices[3 * i + 0] > max[0]) max[0] = vertices[3 * i + 0];

      if (vertices[3 * i + 1] < min[1]) min[1] = vertices[3 * i + 1];
      else if (vertices[3 * i + 1] > max[1]) max[1] = vertices[3 * i + 1];

      if (vertices[3 * i + 2] < min[2]) min[2] = vertices[3 * i + 2];
      else if (vertices[3 * i + 2] > max[2]) max[2] = vertices[3 * i + 2];
    }

    input2.min[0] = min[0];
    input2.min[1] = min[1];
    input2.min[2] = min[2];

    input2.max[0] = max[0];
    input2.max[1] = max[1];
    input2.max[2] = max[2];

    DualConOutput2* output = dualcon(
                  &input2,
                  dualcon_alloc_output2,
                  dualcon_add_vert2,
                  dualcon_add_quad2,
                  (DualConFlags)1,
                  2,
                  threshold,
                  hermit_num,
                  scale,
                  octree_depth);

    printf("Output vertices: %d Output faces: %d\n", output->curvert, output->curface);

    FILE* file = fopen(path, "wb");

    for (int i = 0; i < output->curvert; i++)
      fprintf(file, "v %f %f %f\n", output->verts[i].co[0], output->verts[i].co[1], output->verts[i].co[2]);

    for (int i = 0; i < output->curface; i++)
      fprintf(file, "f %d %d %d %d\n", 
              output->loops[4 * i + 0]+1, 
              output->loops[4 * i + 1]+1, 
              output->loops[4 * i + 2]+1, 
              output->loops[4 * i + 3]+1);

    fflush(file);

    printf("Finished writing OBJ file\n");

    fclose(file);

    MEM_freeN(indexdata);

    MEM_freeN(vertices);

    MEM_freeN(loopdata);;

    MEM_freeN(output);

    return 0;
}
