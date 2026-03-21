
#pragma once

namespace tyon
{

/// all attributes are baked when copied to the graphiccs layer
// be mindful of this
struct fmesh
{
    fstring name;
    std::vector<v3> vertex_buffer;
    std::vector<fuint32> vertex_index_buffer;
    std::vector<v4> vertex_color_buffer;
    i32 face_count = 0;
    i32 vertex_count = 0;
    i32 index_count = 0;
    i32 color_count = 0;
};

}
