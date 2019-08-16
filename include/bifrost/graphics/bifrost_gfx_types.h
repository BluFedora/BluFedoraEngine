#ifndef BIFROST_GFX_TYPES_H
#define BIFROST_GFX_TYPES_H

#if __cplusplus
extern "C" {
#endif
typedef enum BifrostIndexType_t
{
  BIFROST_INDEX_TYPE_UINT16,
  BIFROST_INDEX_TYPE_UINT32

} BifrostIndexType;

typedef enum BifrostVertexFormatAttribute_t
{
  // NOTE(Shareef): 32-bit Float
  BIFROST_VFA_FLOAT32_4,
  BIFROST_VFA_FLOAT32_3,
  BIFROST_VFA_FLOAT32_2,
  BIFROST_VFA_FLOAT32_1,
  // NOTE(Shareef): 32-bit Unsigned Int
  BIFROST_VFA_UINT32_4,
  BIFROST_VFA_UINT32_3,
  BIFROST_VFA_UINT32_2,
  BIFROST_VFA_UINT32_1,
  // NOTE(Shareef): 32-bit Signed Int
  BIFROST_VFA_SINT32_4,
  BIFROST_VFA_SINT32_3,
  BIFROST_VFA_SINT32_2,
  BIFROST_VFA_SINT32_1,
  // NOTE(Shareef): 16-bit Unsigned Int
  BIFROST_VFA_USHORT16_4,
  BIFROST_VFA_USHORT16_3,
  BIFROST_VFA_USHORT16_2,
  BIFROST_VFA_USHORT16_1,
  // NOTE(Shareef): 16-bit Signed Int
  BIFROST_VFA_SSHORT16_4,
  BIFROST_VFA_SSHORT16_3,
  BIFROST_VFA_SSHORT16_2,
  BIFROST_VFA_SSHORT16_1,
  // NOTE(Shareef): 8-bit Unsigned Int
  BIFROST_VFA_UCHAR8_4,
  BIFROST_VFA_UCHAR8_3,
  BIFROST_VFA_UCHAR8_2,
  BIFROST_VFA_UCHAR8_1,
  // NOTE(Shareef): 8-bit Signed Int
  BIFROST_VFA_SCHAR8_4,
  BIFROST_VFA_SCHAR8_3,
  BIFROST_VFA_SCHAR8_2,
  BIFROST_VFA_SCHAR8_1,
  // NOTE(Shareef):
  //   For Color packed as 8 bit chars but need to be converted to floats
  BIFROST_VFA_UCHAR8_4_UNORM

} BifrostVertexFormatAttribute;

// [https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkFormat.html]
typedef enum BifrostImageFormat_t
{
  BIFROST_IMAGE_FORMAT_UNDEFINED                                  = 0,
  BIFROST_IMAGE_FORMAT_R4G4_UNORM_PACK8                           = 1,
  BIFROST_IMAGE_FORMAT_R4G4B4A4_UNORM_PACK16                      = 2,
  BIFROST_IMAGE_FORMAT_B4G4R4A4_UNORM_PACK16                      = 3,
  BIFROST_IMAGE_FORMAT_R5G6B5_UNORM_PACK16                        = 4,
  BIFROST_IMAGE_FORMAT_B5G6R5_UNORM_PACK16                        = 5,
  BIFROST_IMAGE_FORMAT_R5G5B5A1_UNORM_PACK16                      = 6,
  BIFROST_IMAGE_FORMAT_B5G5R5A1_UNORM_PACK16                      = 7,
  BIFROST_IMAGE_FORMAT_A1R5G5B5_UNORM_PACK16                      = 8,
  BIFROST_IMAGE_FORMAT_R8_UNORM                                   = 9,
  BIFROST_IMAGE_FORMAT_R8_SNORM                                   = 10,
  BIFROST_IMAGE_FORMAT_R8_USCALED                                 = 11,
  BIFROST_IMAGE_FORMAT_R8_SSCALED                                 = 12,
  BIFROST_IMAGE_FORMAT_R8_UINT                                    = 13,
  BIFROST_IMAGE_FORMAT_R8_SINT                                    = 14,
  BIFROST_IMAGE_FORMAT_R8_SRGB                                    = 15,
  BIFROST_IMAGE_FORMAT_R8G8_UNORM                                 = 16,
  BIFROST_IMAGE_FORMAT_R8G8_SNORM                                 = 17,
  BIFROST_IMAGE_FORMAT_R8G8_USCALED                               = 18,
  BIFROST_IMAGE_FORMAT_R8G8_SSCALED                               = 19,
  BIFROST_IMAGE_FORMAT_R8G8_UINT                                  = 20,
  BIFROST_IMAGE_FORMAT_R8G8_SINT                                  = 21,
  BIFROST_IMAGE_FORMAT_R8G8_SRGB                                  = 22,
  BIFROST_IMAGE_FORMAT_R8G8B8_UNORM                               = 23,
  BIFROST_IMAGE_FORMAT_R8G8B8_SNORM                               = 24,
  BIFROST_IMAGE_FORMAT_R8G8B8_USCALED                             = 25,
  BIFROST_IMAGE_FORMAT_R8G8B8_SSCALED                             = 26,
  BIFROST_IMAGE_FORMAT_R8G8B8_UINT                                = 27,
  BIFROST_IMAGE_FORMAT_R8G8B8_SINT                                = 28,
  BIFROST_IMAGE_FORMAT_R8G8B8_SRGB                                = 29,
  BIFROST_IMAGE_FORMAT_B8G8R8_UNORM                               = 30,
  BIFROST_IMAGE_FORMAT_B8G8R8_SNORM                               = 31,
  BIFROST_IMAGE_FORMAT_B8G8R8_USCALED                             = 32,
  BIFROST_IMAGE_FORMAT_B8G8R8_SSCALED                             = 33,
  BIFROST_IMAGE_FORMAT_B8G8R8_UINT                                = 34,
  BIFROST_IMAGE_FORMAT_B8G8R8_SINT                                = 35,
  BIFROST_IMAGE_FORMAT_B8G8R8_SRGB                                = 36,
  BIFROST_IMAGE_FORMAT_R8G8B8A8_UNORM                             = 37,
  BIFROST_IMAGE_FORMAT_R8G8B8A8_SNORM                             = 38,
  BIFROST_IMAGE_FORMAT_R8G8B8A8_USCALED                           = 39,
  BIFROST_IMAGE_FORMAT_R8G8B8A8_SSCALED                           = 40,
  BIFROST_IMAGE_FORMAT_R8G8B8A8_UINT                              = 41,
  BIFROST_IMAGE_FORMAT_R8G8B8A8_SINT                              = 42,
  BIFROST_IMAGE_FORMAT_R8G8B8A8_SRGB                              = 43,
  BIFROST_IMAGE_FORMAT_B8G8R8A8_UNORM                             = 44,
  BIFROST_IMAGE_FORMAT_B8G8R8A8_SNORM                             = 45,
  BIFROST_IMAGE_FORMAT_B8G8R8A8_USCALED                           = 46,
  BIFROST_IMAGE_FORMAT_B8G8R8A8_SSCALED                           = 47,
  BIFROST_IMAGE_FORMAT_B8G8R8A8_UINT                              = 48,
  BIFROST_IMAGE_FORMAT_B8G8R8A8_SINT                              = 49,
  BIFROST_IMAGE_FORMAT_B8G8R8A8_SRGB                              = 50,
  BIFROST_IMAGE_FORMAT_A8B8G8R8_UNORM_PACK32                      = 51,
  BIFROST_IMAGE_FORMAT_A8B8G8R8_SNORM_PACK32                      = 52,
  BIFROST_IMAGE_FORMAT_A8B8G8R8_USCALED_PACK32                    = 53,
  BIFROST_IMAGE_FORMAT_A8B8G8R8_SSCALED_PACK32                    = 54,
  BIFROST_IMAGE_FORMAT_A8B8G8R8_UINT_PACK32                       = 55,
  BIFROST_IMAGE_FORMAT_A8B8G8R8_SINT_PACK32                       = 56,
  BIFROST_IMAGE_FORMAT_A8B8G8R8_SRGB_PACK32                       = 57,
  BIFROST_IMAGE_FORMAT_A2R10G10B10_UNORM_PACK32                   = 58,
  BIFROST_IMAGE_FORMAT_A2R10G10B10_SNORM_PACK32                   = 59,
  BIFROST_IMAGE_FORMAT_A2R10G10B10_USCALED_PACK32                 = 60,
  BIFROST_IMAGE_FORMAT_A2R10G10B10_SSCALED_PACK32                 = 61,
  BIFROST_IMAGE_FORMAT_A2R10G10B10_UINT_PACK32                    = 62,
  BIFROST_IMAGE_FORMAT_A2R10G10B10_SINT_PACK32                    = 63,
  BIFROST_IMAGE_FORMAT_A2B10G10R10_UNORM_PACK32                   = 64,
  BIFROST_IMAGE_FORMAT_A2B10G10R10_SNORM_PACK32                   = 65,
  BIFROST_IMAGE_FORMAT_A2B10G10R10_USCALED_PACK32                 = 66,
  BIFROST_IMAGE_FORMAT_A2B10G10R10_SSCALED_PACK32                 = 67,
  BIFROST_IMAGE_FORMAT_A2B10G10R10_UINT_PACK32                    = 68,
  BIFROST_IMAGE_FORMAT_A2B10G10R10_SINT_PACK32                    = 69,
  BIFROST_IMAGE_FORMAT_R16_UNORM                                  = 70,
  BIFROST_IMAGE_FORMAT_R16_SNORM                                  = 71,
  BIFROST_IMAGE_FORMAT_R16_USCALED                                = 72,
  BIFROST_IMAGE_FORMAT_R16_SSCALED                                = 73,
  BIFROST_IMAGE_FORMAT_R16_UINT                                   = 74,
  BIFROST_IMAGE_FORMAT_R16_SINT                                   = 75,
  BIFROST_IMAGE_FORMAT_R16_SFLOAT                                 = 76,
  BIFROST_IMAGE_FORMAT_R16G16_UNORM                               = 77,
  BIFROST_IMAGE_FORMAT_R16G16_SNORM                               = 78,
  BIFROST_IMAGE_FORMAT_R16G16_USCALED                             = 79,
  BIFROST_IMAGE_FORMAT_R16G16_SSCALED                             = 80,
  BIFROST_IMAGE_FORMAT_R16G16_UINT                                = 81,
  BIFROST_IMAGE_FORMAT_R16G16_SINT                                = 82,
  BIFROST_IMAGE_FORMAT_R16G16_SFLOAT                              = 83,
  BIFROST_IMAGE_FORMAT_R16G16B16_UNORM                            = 84,
  BIFROST_IMAGE_FORMAT_R16G16B16_SNORM                            = 85,
  BIFROST_IMAGE_FORMAT_R16G16B16_USCALED                          = 86,
  BIFROST_IMAGE_FORMAT_R16G16B16_SSCALED                          = 87,
  BIFROST_IMAGE_FORMAT_R16G16B16_UINT                             = 88,
  BIFROST_IMAGE_FORMAT_R16G16B16_SINT                             = 89,
  BIFROST_IMAGE_FORMAT_R16G16B16_SFLOAT                           = 90,
  BIFROST_IMAGE_FORMAT_R16G16B16A16_UNORM                         = 91,
  BIFROST_IMAGE_FORMAT_R16G16B16A16_SNORM                         = 92,
  BIFROST_IMAGE_FORMAT_R16G16B16A16_USCALED                       = 93,
  BIFROST_IMAGE_FORMAT_R16G16B16A16_SSCALED                       = 94,
  BIFROST_IMAGE_FORMAT_R16G16B16A16_UINT                          = 95,
  BIFROST_IMAGE_FORMAT_R16G16B16A16_SINT                          = 96,
  BIFROST_IMAGE_FORMAT_R16G16B16A16_SFLOAT                        = 97,
  BIFROST_IMAGE_FORMAT_R32_UINT                                   = 98,
  BIFROST_IMAGE_FORMAT_R32_SINT                                   = 99,
  BIFROST_IMAGE_FORMAT_R32_SFLOAT                                 = 100,
  BIFROST_IMAGE_FORMAT_R32G32_UINT                                = 101,
  BIFROST_IMAGE_FORMAT_R32G32_SINT                                = 102,
  BIFROST_IMAGE_FORMAT_R32G32_SFLOAT                              = 103,
  BIFROST_IMAGE_FORMAT_R32G32B32_UINT                             = 104,
  BIFROST_IMAGE_FORMAT_R32G32B32_SINT                             = 105,
  BIFROST_IMAGE_FORMAT_R32G32B32_SFLOAT                           = 106,
  BIFROST_IMAGE_FORMAT_R32G32B32A32_UINT                          = 107,
  BIFROST_IMAGE_FORMAT_R32G32B32A32_SINT                          = 108,
  BIFROST_IMAGE_FORMAT_R32G32B32A32_SFLOAT                        = 109,
  BIFROST_IMAGE_FORMAT_R64_UINT                                   = 110,
  BIFROST_IMAGE_FORMAT_R64_SINT                                   = 111,
  BIFROST_IMAGE_FORMAT_R64_SFLOAT                                 = 112,
  BIFROST_IMAGE_FORMAT_R64G64_UINT                                = 113,
  BIFROST_IMAGE_FORMAT_R64G64_SINT                                = 114,
  BIFROST_IMAGE_FORMAT_R64G64_SFLOAT                              = 115,
  BIFROST_IMAGE_FORMAT_R64G64B64_UINT                             = 116,
  BIFROST_IMAGE_FORMAT_R64G64B64_SINT                             = 117,
  BIFROST_IMAGE_FORMAT_R64G64B64_SFLOAT                           = 118,
  BIFROST_IMAGE_FORMAT_R64G64B64A64_UINT                          = 119,
  BIFROST_IMAGE_FORMAT_R64G64B64A64_SINT                          = 120,
  BIFROST_IMAGE_FORMAT_R64G64B64A64_SFLOAT                        = 121,
  BIFROST_IMAGE_FORMAT_B10G11R11_UFLOAT_PACK32                    = 122,
  BIFROST_IMAGE_FORMAT_E5B9G9R9_UFLOAT_PACK32                     = 123,
  BIFROST_IMAGE_FORMAT_D16_UNORM                                  = 124,
  BIFROST_IMAGE_FORMAT_X8_D24_UNORM_PACK32                        = 125,
  BIFROST_IMAGE_FORMAT_D32_SFLOAT                                 = 126,
  BIFROST_IMAGE_FORMAT_S8_UINT                                    = 127,
  BIFROST_IMAGE_FORMAT_D16_UNORM_S8_UINT                          = 128,
  BIFROST_IMAGE_FORMAT_D24_UNORM_S8_UINT                          = 129,
  BIFROST_IMAGE_FORMAT_D32_SFLOAT_S8_UINT                         = 130,
  BIFROST_IMAGE_FORMAT_BC1_RGB_UNORM_BLOCK                        = 131,
  BIFROST_IMAGE_FORMAT_BC1_RGB_SRGB_BLOCK                         = 132,
  BIFROST_IMAGE_FORMAT_BC1_RGBA_UNORM_BLOCK                       = 133,
  BIFROST_IMAGE_FORMAT_BC1_RGBA_SRGB_BLOCK                        = 134,
  BIFROST_IMAGE_FORMAT_BC2_UNORM_BLOCK                            = 135,
  BIFROST_IMAGE_FORMAT_BC2_SRGB_BLOCK                             = 136,
  BIFROST_IMAGE_FORMAT_BC3_UNORM_BLOCK                            = 137,
  BIFROST_IMAGE_FORMAT_BC3_SRGB_BLOCK                             = 138,
  BIFROST_IMAGE_FORMAT_BC4_UNORM_BLOCK                            = 139,
  BIFROST_IMAGE_FORMAT_BC4_SNORM_BLOCK                            = 140,
  BIFROST_IMAGE_FORMAT_BC5_UNORM_BLOCK                            = 141,
  BIFROST_IMAGE_FORMAT_BC5_SNORM_BLOCK                            = 142,
  BIFROST_IMAGE_FORMAT_BC6H_UFLOAT_BLOCK                          = 143,
  BIFROST_IMAGE_FORMAT_BC6H_SFLOAT_BLOCK                          = 144,
  BIFROST_IMAGE_FORMAT_BC7_UNORM_BLOCK                            = 145,
  BIFROST_IMAGE_FORMAT_BC7_SRGB_BLOCK                             = 146,
  BIFROST_IMAGE_FORMAT_ETC2_R8G8B8_UNORM_BLOCK                    = 147,
  BIFROST_IMAGE_FORMAT_ETC2_R8G8B8_SRGB_BLOCK                     = 148,
  BIFROST_IMAGE_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK                  = 149,
  BIFROST_IMAGE_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK                   = 150,
  BIFROST_IMAGE_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK                  = 151,
  BIFROST_IMAGE_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK                   = 152,
  BIFROST_IMAGE_FORMAT_EAC_R11_UNORM_BLOCK                        = 153,
  BIFROST_IMAGE_FORMAT_EAC_R11_SNORM_BLOCK                        = 154,
  BIFROST_IMAGE_FORMAT_EAC_R11G11_UNORM_BLOCK                     = 155,
  BIFROST_IMAGE_FORMAT_EAC_R11G11_SNORM_BLOCK                     = 156,
  BIFROST_IMAGE_FORMAT_ASTC_4x4_UNORM_BLOCK                       = 157,
  BIFROST_IMAGE_FORMAT_ASTC_4x4_SRGB_BLOCK                        = 158,
  BIFROST_IMAGE_FORMAT_ASTC_5x4_UNORM_BLOCK                       = 159,
  BIFROST_IMAGE_FORMAT_ASTC_5x4_SRGB_BLOCK                        = 160,
  BIFROST_IMAGE_FORMAT_ASTC_5x5_UNORM_BLOCK                       = 161,
  BIFROST_IMAGE_FORMAT_ASTC_5x5_SRGB_BLOCK                        = 162,
  BIFROST_IMAGE_FORMAT_ASTC_6x5_UNORM_BLOCK                       = 163,
  BIFROST_IMAGE_FORMAT_ASTC_6x5_SRGB_BLOCK                        = 164,
  BIFROST_IMAGE_FORMAT_ASTC_6x6_UNORM_BLOCK                       = 165,
  BIFROST_IMAGE_FORMAT_ASTC_6x6_SRGB_BLOCK                        = 166,
  BIFROST_IMAGE_FORMAT_ASTC_8x5_UNORM_BLOCK                       = 167,
  BIFROST_IMAGE_FORMAT_ASTC_8x5_SRGB_BLOCK                        = 168,
  BIFROST_IMAGE_FORMAT_ASTC_8x6_UNORM_BLOCK                       = 169,
  BIFROST_IMAGE_FORMAT_ASTC_8x6_SRGB_BLOCK                        = 170,
  BIFROST_IMAGE_FORMAT_ASTC_8x8_UNORM_BLOCK                       = 171,
  BIFROST_IMAGE_FORMAT_ASTC_8x8_SRGB_BLOCK                        = 172,
  BIFROST_IMAGE_FORMAT_ASTC_10x5_UNORM_BLOCK                      = 173,
  BIFROST_IMAGE_FORMAT_ASTC_10x5_SRGB_BLOCK                       = 174,
  BIFROST_IMAGE_FORMAT_ASTC_10x6_UNORM_BLOCK                      = 175,
  BIFROST_IMAGE_FORMAT_ASTC_10x6_SRGB_BLOCK                       = 176,
  BIFROST_IMAGE_FORMAT_ASTC_10x8_UNORM_BLOCK                      = 177,
  BIFROST_IMAGE_FORMAT_ASTC_10x8_SRGB_BLOCK                       = 178,
  BIFROST_IMAGE_FORMAT_ASTC_10x10_UNORM_BLOCK                     = 179,
  BIFROST_IMAGE_FORMAT_ASTC_10x10_SRGB_BLOCK                      = 180,
  BIFROST_IMAGE_FORMAT_ASTC_12x10_UNORM_BLOCK                     = 181,
  BIFROST_IMAGE_FORMAT_ASTC_12x10_SRGB_BLOCK                      = 182,
  BIFROST_IMAGE_FORMAT_ASTC_12x12_UNORM_BLOCK                     = 183,
  BIFROST_IMAGE_FORMAT_ASTC_12x12_SRGB_BLOCK                      = 184,
  BIFROST_IMAGE_FORMAT_G8B8G8R8_422_UNORM                         = 1000156000,
  BIFROST_IMAGE_FORMAT_B8G8R8G8_422_UNORM                         = 1000156001,
  BIFROST_IMAGE_FORMAT_G8_B8_R8_3PLANE_420_UNORM                  = 1000156002,
  BIFROST_IMAGE_FORMAT_G8_B8R8_2PLANE_420_UNORM                   = 1000156003,
  BIFROST_IMAGE_FORMAT_G8_B8_R8_3PLANE_422_UNORM                  = 1000156004,
  BIFROST_IMAGE_FORMAT_G8_B8R8_2PLANE_422_UNORM                   = 1000156005,
  BIFROST_IMAGE_FORMAT_G8_B8_R8_3PLANE_444_UNORM                  = 1000156006,
  BIFROST_IMAGE_FORMAT_R10X6_UNORM_PACK16                         = 1000156007,
  BIFROST_IMAGE_FORMAT_R10X6G10X6_UNORM_2PACK16                   = 1000156008,
  BIFROST_IMAGE_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16         = 1000156009,
  BIFROST_IMAGE_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16     = 1000156010,
  BIFROST_IMAGE_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16     = 1000156011,
  BIFROST_IMAGE_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 = 1000156012,
  BIFROST_IMAGE_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16  = 1000156013,
  BIFROST_IMAGE_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 = 1000156014,
  BIFROST_IMAGE_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16  = 1000156015,
  BIFROST_IMAGE_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 = 1000156016,
  BIFROST_IMAGE_FORMAT_R12X4_UNORM_PACK16                         = 1000156017,
  BIFROST_IMAGE_FORMAT_R12X4G12X4_UNORM_2PACK16                   = 1000156018,
  BIFROST_IMAGE_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16         = 1000156019,
  BIFROST_IMAGE_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16     = 1000156020,
  BIFROST_IMAGE_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16     = 1000156021,
  BIFROST_IMAGE_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 = 1000156022,
  BIFROST_IMAGE_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16  = 1000156023,
  BIFROST_IMAGE_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 = 1000156024,
  BIFROST_IMAGE_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16  = 1000156025,
  BIFROST_IMAGE_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 = 1000156026,
  BIFROST_IMAGE_FORMAT_G16B16G16R16_422_UNORM                     = 1000156027,
  BIFROST_IMAGE_FORMAT_B16G16R16G16_422_UNORM                     = 1000156028,
  BIFROST_IMAGE_FORMAT_G16_B16_R16_3PLANE_420_UNORM               = 1000156029,
  BIFROST_IMAGE_FORMAT_G16_B16R16_2PLANE_420_UNORM                = 1000156030,
  BIFROST_IMAGE_FORMAT_G16_B16_R16_3PLANE_422_UNORM               = 1000156031,
  BIFROST_IMAGE_FORMAT_G16_B16R16_2PLANE_422_UNORM                = 1000156032,
  BIFROST_IMAGE_FORMAT_G16_B16_R16_3PLANE_444_UNORM               = 1000156033,
  BIFROST_IMAGE_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG                = 1000054000,
  BIFROST_IMAGE_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG                = 1000054001,
  BIFROST_IMAGE_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG                = 1000054002,
  BIFROST_IMAGE_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG                = 1000054003,
  BIFROST_IMAGE_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG                 = 1000054004,
  BIFROST_IMAGE_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG                 = 1000054005,
  BIFROST_IMAGE_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG                 = 1000054006,
  BIFROST_IMAGE_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG                 = 1000054007,
  BIFROST_IMAGE_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT                  = 1000066000,
  BIFROST_IMAGE_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT                  = 1000066001,
  BIFROST_IMAGE_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT                  = 1000066002,
  BIFROST_IMAGE_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT                  = 1000066003,
  BIFROST_IMAGE_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT                  = 1000066004,
  BIFROST_IMAGE_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT                  = 1000066005,
  BIFROST_IMAGE_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT                  = 1000066006,
  BIFROST_IMAGE_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT                  = 1000066007,
  BIFROST_IMAGE_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT                 = 1000066008,
  BIFROST_IMAGE_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT                 = 1000066009,
  BIFROST_IMAGE_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT                 = 1000066010,
  BIFROST_IMAGE_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT                = 1000066011,
  BIFROST_IMAGE_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT                = 1000066012,
  BIFROST_IMAGE_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT                = 1000066013,
  BIFROST_IMAGE_FORMAT_MAX_ENUM                                   = 0x7FFFFFFF,

} BifrostImageFormat;

typedef enum BifrostPipelineStageFlags_t
{
  BIFROST_PIPELINE_STAGE_TOP_OF_PIPE_BIT                    = 0x00000001,
  BIFROST_PIPELINE_STAGE_DRAW_INDIRECT_BIT                  = 0x00000002,
  BIFROST_PIPELINE_STAGE_VERTEX_INPUT_BIT                   = 0x00000004,
  BIFROST_PIPELINE_STAGE_VERTEX_SHADER_BIT                  = 0x00000008,
  BIFROST_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT    = 0x00000010,
  BIFROST_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = 0x00000020,
  BIFROST_PIPELINE_STAGE_GEOMETRY_SHADER_BIT                = 0x00000040,
  BIFROST_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                = 0x00000080,
  BIFROST_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT           = 0x00000100,
  BIFROST_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT            = 0x00000200,
  BIFROST_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT        = 0x00000400,
  BIFROST_PIPELINE_STAGE_COMPUTE_SHADER_BIT                 = 0x00000800,
  BIFROST_PIPELINE_STAGE_TRANSFER_BIT                       = 0x00001000,
  BIFROST_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT                 = 0x00002000,
  BIFROST_PIPELINE_STAGE_HOST_BIT                           = 0x00004000,
  BIFROST_PIPELINE_STAGE_ALL_GRAPHICS_BIT                   = 0x00008000,
  BIFROST_PIPELINE_STAGE_ALL_COMMANDS_BIT                   = 0x00010000,

} BifrostPipelineStageFlags;

typedef enum BifrostAccessFlags_t
{
  BIFROST_ACCESS_INDIRECT_COMMAND_READ_BIT          = 0x00000001,
  BIFROST_ACCESS_INDEX_READ_BIT                     = 0x00000002,
  BIFROST_ACCESS_VERTEX_ATTRIBUTE_READ_BIT          = 0x00000004,
  BIFROST_ACCESS_UNIFORM_READ_BIT                   = 0x00000008,
  BIFROST_ACCESS_INPUT_ATTACHMENT_READ_BIT          = 0x00000010,
  BIFROST_ACCESS_SHADER_READ_BIT                    = 0x00000020,
  BIFROST_ACCESS_SHADER_WRITE_BIT                   = 0x00000040,
  BIFROST_ACCESS_COLOR_ATTACHMENT_READ_BIT          = 0x00000080,
  BIFROST_ACCESS_COLOR_ATTACHMENT_WRITE_BIT         = 0x00000100,
  BIFROST_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT  = 0x00000200,
  BIFROST_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = 0x00000400,
  BIFROST_ACCESS_TRANSFER_READ_BIT                  = 0x00000800,
  BIFROST_ACCESS_TRANSFER_WRITE_BIT                 = 0x00001000,
  BIFROST_ACCESS_HOST_READ_BIT                      = 0x00002000,
  BIFROST_ACCESS_HOST_WRITE_BIT                     = 0x00004000,
  BIFROST_ACCESS_MEMORY_READ_BIT                    = 0x00008000,
  BIFROST_ACCESS_MEMORY_WRITE_BIT                   = 0x00010000,

} BifrostAccessFlags;

typedef enum BifrostSampleFlags_t
{
  BIFROST_SAMPLE_1  = 0x00000001,
  BIFROST_SAMPLE_2  = 0x00000002,
  BIFROST_SAMPLE_4  = 0x00000004,
  BIFROST_SAMPLE_8  = 0x00000008,
  BIFROST_SAMPLE_16 = 0x00000010,
  BIFROST_SAMPLE_32 = 0x00000020,
  BIFROST_SAMPLE_64 = 0x00000040,

} BifrostSampleFlags;

typedef enum BifrostImageLayout_t
{
  BIFROST_IMAGE_LAYOUT_UNDEFINED                                  = 0,
  BIFROST_IMAGE_LAYOUT_GENERAL                                    = 1,
  BIFROST_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL                   = 2,
  BIFROST_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL           = 3,
  BIFROST_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL            = 4,
  BIFROST_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                   = 5,
  BIFROST_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL                       = 6,
  BIFROST_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL                       = 7,
  BIFROST_IMAGE_LAYOUT_PRE_INITIALIZED                            = 8,
  BIFROST_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL = 1000117000,
  BIFROST_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL = 1000117001,
  BIFROST_IMAGE_LAYOUT_PRESENT_SRC_KHR                            = 1000001002,
  BIFROST_IMAGE_LAYOUT_SHARED_PRESENT_KHR                         = 1000111000,

} BifrostImageLayout;

typedef enum BifrostGfxQueueType_t
{
  BIFROST_GFX_QUEUE_GRAPHICS,
  BIFROST_GFX_QUEUE_COMPUTE,
  BIFROST_GFX_QUEUE_TRANSFER,
  BIFROST_GFX_QUEUE_PRESENT,
  BIFROST_GFX_QUEUE_MAX,

} BifrostGfxQueueType;
#if __cplusplus
}
#endif

#endif /* BIFROST_GFX_TYPES_H */
