/******************************************************************************/
/*!
 * @file   bf_anim2d_export.hpp
 * @author Shareef Abdoul-Raheem (https://blufedora.github.io/)
 * @brief
 *   Export the 2D Animation API to a dll.
 *
 * @version 0.0.1
 * @date    2021-03-09
 *
 * @copyright Copyright (c) 2021
 */
/******************************************************************************/
#ifndef BF_SPRITE_ANIM_API_H
#define BF_SPRITE_ANIM_API_H

#if defined _WIN32 || defined __CYGWIN__
#ifdef BF_ANIM2D_EXPORT

#ifdef __GNUC__
#define BF_ANIM2D_API __attribute__((dllexport))
#else
#define BF_ANIM2D_API __declspec(dllexport)
#endif

#elif defined(BF_ANIM2D_EXPORT_STATIC)

#define BF_ANIM2D_API

#else

#ifdef __GNUC__
#define BF_ANIM2D_API __attribute__((dllimport))
#else
#define BF_ANIM2D_API __declspec(dllimport)
#endif

#endif

#define BF_ANIM2D_NOAPI

#else
#if __GNUC__ >= 4
#define BF_ANIM2D_API __attribute__((visibility("default")))
#define BF_ANIM2D_NOAPI __attribute__((visibility("hidden")))
#else
#define BF_ANIM2D_API
#define BF_ANIM2D_NOAPI
#endif
#endif

#ifdef __GNUC__
#define BF_CDECL __attribute__((__cdecl__))
#else
#define BF_CDECL __cdecl
#endif

#endif /* BF_SPRITE_ANIM_API_H */

/******************************************************************************/
/*
  MIT License

  Copyright (c) 2021 Shareef Abdoul-Raheem

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
/******************************************************************************/
