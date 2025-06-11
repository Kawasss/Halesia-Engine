// all layouts can be declared here with macros, in order to allow easy management of said layouts

#define reserved_set 4

#define bindless_texture_size 1000
#define bindless_textures binding = 0, set = reserved_set
#define mesh_instances binding = 1, set = reserved_set
#define vertex_buffer binding = 2, set = reserved_set
#define index_buffer binding = 3, set = reserved_set