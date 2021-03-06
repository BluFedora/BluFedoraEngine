﻿cmake_minimum_required(VERSION 3.8)

project(BF_AssetIO VERSION 0.0.1 DESCRIPTION "")

add_library(
  "${PROJECT_NAME}" STATIC

  "${PROJECT_SOURCE_DIR}/include/bf/asset_io/bf_asset_map.hpp"
  "${PROJECT_SOURCE_DIR}/include/bf/asset_io/bf_base_asset.hpp"
  "${PROJECT_SOURCE_DIR}/include/bf/asset_io/bf_class_id.hpp"
  "${PROJECT_SOURCE_DIR}/include/bf/asset_io/bf_document.hpp"
  "${PROJECT_SOURCE_DIR}/include/bf/asset_io/bf_gfx_assets.hpp"
  "${PROJECT_SOURCE_DIR}/include/bf/asset_io/bf_iasset_importer.hpp" 
  "${PROJECT_SOURCE_DIR}/include/bf/asset_io/bf_iserializer.hpp"
  "${PROJECT_SOURCE_DIR}/include/bf/asset_io/bf_model_loader.hpp"
  "${PROJECT_SOURCE_DIR}/include/bf/asset_io/bf_path_manip.hpp"
  "${PROJECT_SOURCE_DIR}/include/bf/asset_io/bf_spritesheet_asset.hpp"
 
  "${PROJECT_SOURCE_DIR}/src/bf_asset_map.cpp"
  "${PROJECT_SOURCE_DIR}/src/bf_model_loader.cpp"
  "${PROJECT_SOURCE_DIR}/src/bf_gfx_assets.cpp"
  "${PROJECT_SOURCE_DIR}/src/bf_base_asset.cpp"
  "${PROJECT_SOURCE_DIR}/src/bf_document.cpp"
)

target_include_directories(
  "${PROJECT_NAME}"

  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/../../Engine/Runtime/include" # TODO(SR): This is a hack so should be removed.

  PRIVATE
    "${PROJECT_SOURCE_DIR}/lib/include"
)

set(BF_ASSETIO_LIBS "")

if (WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(
        BF_ASSETIO_LIBS
        "${BF_ASSETIO_LIBS}"
        "${PROJECT_SOURCE_DIR}/lib/win64/assimp-vc142-mtd.lib"
      )
    else()
      set(
        BF_ASSETIO_LIBS
        "${BF_ASSETIO_LIBS}"
        "${PROJECT_SOURCE_DIR}/lib/win64/assimp-vc142-mt.lib"
      )
    endif()
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
     if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(
        BF_ASSETIO_LIBS
        "${BF_ASSETIO_LIBS}"
        "${PROJECT_SOURCE_DIR}/lib/win32/assimp-vc142-mtd.lib"
      )
    else()
      set(
        BF_ASSETIO_LIBS
        "${BF_ASSETIO_LIBS}"
        "${PROJECT_SOURCE_DIR}/lib/win32/assimp-vc142-mt.lib"
      )
    endif()
  endif()
elseif(APPLE)
  message(SEND_ERROR "MacOS needs a build of the Assimp Model Loading Library.")
else()
  message(SEND_ERROR "Linux needs a build of the Assimp Model Loading Library.")
endif()

# add_subdirectory(lib/assimp)

target_link_libraries(
  "${PROJECT_NAME}"
  PUBLIC
    BF_Core
    BF_Graphics

    BF_Math_shared
    BF_Memory_interface
    BF_TMPUtils

    BF_DataStructuresCxx
    
  PRIVATE
    "${BF_ASSETIO_LIBS}"
)
