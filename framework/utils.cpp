// -----------------------------------------------------------------------------
// Copyright  : (C) 2014 Andreas-C. Bernstein
// License    : MIT (see the file LICENSE)
// Maintainer : Andreas-C. Bernstein <andreas.bernstein@uni-weimar.de>
// Stability  : experimental
//
// utils
// -----------------------------------------------------------------------------

#include "utils.hpp"
#include <stdexcept>

#define GLM_FORCE_RADIANS
#include <glm/vec2.hpp>
#include <glm/gtx/perpendicular.hpp>

GLuint loadShader(GLenum type, std::string const& source_str)
{
  GLuint id = glCreateShader(type);
  const char* source = source_str.c_str();
  glShaderSource(id, 1, &source, nullptr);
  glCompileShader(id);

  GLint successful;
  glGetShaderiv(id, GL_COMPILE_STATUS, &successful);

  if (!successful) {
    int length;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
    std::string info(length, ' ');

    glGetShaderInfoLog(id, length, &length, &info[0]);
    throw std::logic_error(info);
  }
  return id;
}

GLuint createProgram(std::string const& v, std::string const& f)
{
  GLuint id = glCreateProgram();

  GLuint vsHandle = loadShader(GL_VERTEX_SHADER, v);
  GLuint fsHandle = loadShader(GL_FRAGMENT_SHADER, f);
  glAttachShader(id, vsHandle);
  glAttachShader(id, fsHandle);
  // schedule for deletion
  glDeleteShader(vsHandle);
  glDeleteShader(fsHandle);

  glLinkProgram(id);
  GLint successful;

  glGetProgramiv(id, GL_LINK_STATUS, &successful);
  if (!successful) {
    int length;
    glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
    std::string info(length, ' ');

    glGetProgramInfoLog(id, length, &length, &info[0]);
    throw std::logic_error(info);
  }
  return id;
}

GLuint updateTexture2D(GLuint tex, unsigned const& width, unsigned const& height,
    const char* data, const GLenum& pixel_format, const GLenum& type)
{
    GLuint error = glGetError();

    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL Error entry updateTexture2D: " << error << std::endl;
    }
    //GLuint tex;
    //glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL Error glTexParameteri: " << error << std::endl;
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, pixel_format, type, data);
    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL Error glTexSubImage2D: " << error << std::endl;
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

GLuint createTexture2D(unsigned const& width, unsigned const& height, const char* data, const GLenum& internal_pixel_format, const GLenum& pixel_format, const GLenum& type)
{
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexImage2D(GL_TEXTURE_2D, 0, internal_pixel_format, width, height, 0, pixel_format,
      type, data);
  
  GLint error = glGetError();
  if (error != GL_NO_ERROR)
  {
      std::cout << "OpenGL Error createTexture2D: " << error << std::endl;
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  
  return tex;
}

GLuint createTexture3D(unsigned const& width, unsigned const& height,
    unsigned const& depth, unsigned const channel_size,
    unsigned const channel_count, const char* data)
{
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_3D, tex);

  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  //glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  //glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (channel_size == 1)
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, width, height, depth, 0, GL_RED,
        GL_UNSIGNED_BYTE, data);

  if (channel_size == 2)
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, width, height, depth, 0, GL_RED,
        GL_UNSIGNED_SHORT, data);
  
  if (channel_size == 4)
      glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, width, height, depth, 0, GL_RED,
      GL_FLOAT, data);

  //glBindTexture(GL_TEXTURE_3D, 0);

  return tex;
}


// inSegment(): determine if a glm::vec2 is inside a segment
//    Input:  a glm::vec2 P, and a collinear segment S
//    Return: 1 = P is inside S
//            0 = P is  not inside S
int
inSegment(glm::vec2 P, glm::vec2 S_p0, glm::vec2 S_p1)
{
	if (S_p0.x != S_p1.x) {    // S is not  vertical
		if (S_p0.x <= P.x && P.x <= S_p1.x)
			return 1;
		if (S_p0.x >= P.x && P.x >= S_p1.x)
			return 1;
	}
	else {    // S is vertical, so test y  coordinate
		if (S_p0.y <= P.y && P.y <= S_p1.y)
			return 1;
		if (S_p0.y >= P.y && P.y >= S_p1.y)
			return 1;
	}
	return 0;
}

int
intersect2D_2Segments(glm::vec2 S1_p0, glm::vec2 S1_p1, glm::vec2 S2_p0, glm::vec2 S2_p1, glm::vec2* I0, glm::vec2* I1)
{
	glm::vec2    u = S1_p1 - S1_p0;
	glm::vec2    v = S2_p1 - S2_p0;
	glm::vec2    w = S1_p0 - S2_p0;
	float     D = glm::perp(u, v);

	// test if  they are parallel (includes either being a glm::vec2)
	if (fabs(D) < SMALL_NUM) {           // S1 and S2 are parallel
		if (perp(u, w) != 0 || perp(v, w) != 0) {
			return 0;                    // they are NOT collinear
		}
		// they are collinear or degenerate
		// check if they are degenerate  glm::vec2s
		float du = dot(u, u);
		float dv = dot(v, v);
		if (du == 0 && dv == 0) {            // both segments are glm::vec2s
			if (S1_p0 != S2_p0)         // they are distinct  glm::vec2s
				return 0;
			*I0 = S1_p0;                 // they are the same glm::vec2
			return 1;
		}
		if (du == 0) {                     // S1 is a single glm::vec2
			if (inSegment(S1_p0, S2) == 0)  // but is not in S2
				return 0;
			*I0 = S1_p0;
			return 1;
		}
		if (dv == 0) {                     // S2 a single glm::vec2
			if (inSegment(S2_p0, S1) == 0)  // but is not in S1
				return 0;
			*I0 = S2_p0;
			return 1;
		}
		// they are collinear segments - get  overlap (or not)
		float t0, t1;                    // endglm::vec2s of S1 in eqn for S2
		glm::vec2 w2 = S1_p1 - S2_p0;
		if (v.x != 0) {
			t0 = w.x / v.x;
			t1 = w2.x / v.x;
		}
		else {
			t0 = w.y / v.y;
			t1 = w2.y / v.y;
		}
		if (t0 > t1) {                   // must have t0 smaller than t1
			float t = t0; t0 = t1; t1 = t;    // swap if not
		}
		if (t0 > 1 || t1 < 0) {
			return 0;      // NO overlap
		}
		t0 = t0<0 ? 0 : t0;               // clip to min 0
		t1 = t1>1 ? 1 : t1;               // clip to max 1
		if (t0 == t1) {                  // intersect is a glm::vec2
			*I0 = S2_p0 + t0 * v;
			return 1;
		}

		// they overlap in a valid subsegment
		*I0 = S2_p0 + t0 * v;
		*I1 = S2_p0 + t1 * v;
		return 2;
	}

	// the segments are skew and may intersect in a glm::vec2
	// get the intersect parameter for S1
	float     sI = perp(v, w) / D;
	if (sI < 0 || sI > 1)                // no intersect with S1
		return 0;

	// get the intersect parameter for S2
	float     tI = perp(u, w) / D;
	if (tI < 0 || tI > 1)                // no intersect with S2
		return 0;

	*I0 = S1_p0 + sI * u;                // compute S1 intersect glm::vec2
	return 1;
}