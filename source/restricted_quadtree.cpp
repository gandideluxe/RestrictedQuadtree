// -----------------------------------------------------------------------------
// Copyright  : (C) 2014 Andreas-C. Bernstein
//                  2015 Sebastian Thiele
// License    : MIT (see the file LICENSE)
// Maintainer : Sebastian Thiele <sebastian.thiele@uni-weimar.de>
// Stability  : experimantal exercise
//
// scivis exercise Example
// -----------------------------------------------------------------------------
#ifdef _MSC_VER
#pragma warning (disable: 4996)         // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#endif

#define _USE_MATH_DEFINES
#include "fensterchen.hpp"
#include <string>
#include <iostream>
#include <sstream>      // std::stringstream
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <chrono>
#include <thread>

///GLM INCLUDES
#define GLM_FORCE_RADIANS
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>

///PROJECT INCLUDES
#include <volume_loader_raw.hpp>
#include <volume_loader_raw_hurrican.hpp>
#include <transfer_function.hpp>
#include <QuadtreeRenderer.hpp>
#include <utils.hpp>
#include <turntable.hpp>
#include <imgui.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>        // stb_image.h for PNG loading
#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))

//-----------------------------------------------------------------------------
// Window Size
//-----------------------------------------------------------------------------
glm::ivec2  g_window_res = glm::ivec2(1920, 1080);

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

#define IM_ARRAYSIZE(_ARR)          ((int)(sizeof(_ARR)/sizeof(*_ARR)))

#undef PI
const float PI = 3.14159265358979323846f;

#ifdef INT_MAX
#define IM_INT_MAX INT_MAX
#else
#define IM_INT_MAX 2147483647
#endif

// Play it nice with Windows users. Notepad in 2014 still doesn't display text data with Unix-style \n.
#ifdef _MSC_VER
#define STR_NEWLINE "\r\n"
#else
#define STR_NEWLINE "\n"
#endif

const std::string g_GUI_file_vertex_shader("../../../source/shader/pass_through_GUI.vert");
const std::string g_GUI_file_fragment_shader("../../../source/shader/pass_through_GUI.frag");

GLuint loadShaders(std::string const& vs, std::string const& fs)
{
    std::string v = readFile(vs);
    std::string f = readFile(fs);
    return createProgram(v, f);
}

Turntable  g_turntable;

///SETUP VOLUME RAYCASTER HERE
// set the volume file
std::string g_file_string = "../../../data/head_w256_h256_d225_c1_b8.raw";


// set backgorund color here
glm::vec3   g_background_color = glm::vec3(1.0f, 1.0f, 1.0f);   //grey

Window g_win(g_window_res);

// Volume Rendering GLSL Program
std::string g_error_message;
bool g_reload_shader_error = false;

GLuint g_samplerID = 0;

//std::vector<GLuint> g_volume_texture_handles;
QuadtreeRenderer q_renderer;
std::vector<Transfer_function> g_transfer_fun;
std::vector<GLuint> g_transfer_texture;
std::vector<glm::uint64> g_transfer_texture_handles;
std::vector<unsigned> g_channels_used;
int g_current_tf = 0;
int g_current_tf_data_value = 0;
bool g_transfer_dirty = true;
bool g_redraw_tf = true;
bool g_lighting_toggle = false;
bool g_shadow_toggle = false;
bool g_opacity_correction_toggle = false;

// imgui variables
static bool g_show_gui = true;

GLuint g_color_tex = 0;

static GLuint fontTex;
static bool mousePressed[2] = { false, false };

static int g_gui_program, vert_handle, frag_handle;
static int texture_location, ortho_location;
static int position_location, uv_location, colour_location;
static size_t vbo_max_size = 20000;
static unsigned int vbo_handle, vao_handle;


//imgui values
bool g_over_gui = false;
bool g_reload_shader = false;
bool g_reload_shader_pressed = false;
bool g_show_transfer_function = false;

int g_task_chosen = 21;
int g_task_chosen_old = g_task_chosen;

bool  g_pause = false;

float g_time_last = 0.0;

bool g_time_pause;

int g_bilinear_interpolation = true;

glm::vec2 g_test_point = glm::vec2(0.3f, 0.3f);

glm::vec2 g_ref_point = glm::vec2(0.5f, 0.2f);
glm::vec2 g_frustrum_points[2] = { glm::vec2(0.4f, 0.8f), glm::vec2(0.7f, 0.7f) };

glm::vec2 g_restriction_line[2] = { glm::vec2(0.4f, 0.4f), glm::vec2(0.5f, 0.5f) };
bool      g_restriction_direction = true;
bool      g_restriction = true;



struct Manipulator
{
    Manipulator()
    : m_turntable()
    , m_mouse_button_pressed(0, 0, 0)
    , m_mouse(0.0f, 0.0f)
    , m_lastMouse(0.0f, 0.0f)
    {}

    glm::mat4 matrix()
    {
        m_turntable.rotate(m_slidelastMouse, m_slideMouse);
        return m_turntable.matrix();
    }

    glm::mat4 matrix(Window const& g_win)
    {
        m_mouse = g_win.mousePosition();
        if (g_win.isButtonPressed(Window::MOUSE_BUTTON_LEFT)) {
            if (!m_mouse_button_pressed[0]) {
                m_mouse_button_pressed[0] = 1;
            }
            m_turntable.rotate(m_lastMouse, m_mouse);
            m_slideMouse = m_mouse;
            m_slidelastMouse = m_lastMouse;
            
        }
        else {
            m_mouse_button_pressed[0] = 0;
            m_turntable.rotate(m_slidelastMouse, m_slideMouse);
            //m_slideMouse *= 0.99f;
            //m_slidelastMouse *= 0.99f;
        }

        if (g_win.isButtonPressed(Window::MOUSE_BUTTON_MIDDLE)) {
            if (!m_mouse_button_pressed[1]) {
                m_mouse_button_pressed[1] = 1;
            }
            m_turntable.pan(m_lastMouse, m_mouse);
        }
        else {
            m_mouse_button_pressed[1] = 0;
        }

        if (g_win.isButtonPressed(Window::MOUSE_BUTTON_RIGHT)) {
            if (!m_mouse_button_pressed[2]) {
                m_mouse_button_pressed[2] = 1;
            }
            m_turntable.zoom(m_lastMouse, m_mouse);
        }
        else {
            m_mouse_button_pressed[2] = 0;
        }

        m_lastMouse = m_mouse;
        return m_turntable.matrix();
    }

private:
    Turntable  m_turntable;
    glm::ivec3 m_mouse_button_pressed;
    glm::vec2  m_mouse;
    glm::vec2  m_lastMouse;
    glm::vec2  m_slideMouse;
    glm::vec2  m_slidelastMouse;
};

// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// If text or lines are blurry when integrating ImGui in your engine:
// - try adjusting ImGui::GetIO().PixelCenterOffset to 0.0f or 0.5f
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
static void ImImpl_RenderDrawLists(ImDrawList** const cmd_lists, int cmd_lists_count)
{
    if (cmd_lists_count == 0)
        return;

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    // Setup texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex);

    // Setup orthographic projection matrix
    const float width = ImGui::GetIO().DisplaySize.x;
    const float height = ImGui::GetIO().DisplaySize.y;
    const float ortho_projection[4][4] =
    {
        { 2.0f / width, 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f / -height, 0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f, 0.0f },
        { -1.0f, 1.0f, 0.0f, 1.0f },
    };
    glUseProgram(g_gui_program);
    glUniform1i(texture_location, 0);
    glUniformMatrix4fv(ortho_location, 1, GL_FALSE, &ortho_projection[0][0]);

    // Grow our buffer according to what we need
    size_t total_vtx_count = 0;
    for (int n = 0; n < cmd_lists_count; n++)
        total_vtx_count += cmd_lists[n]->vtx_buffer.size();
    glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
    size_t neededBufferSize = total_vtx_count * sizeof(ImDrawVert);
    if (neededBufferSize > vbo_max_size)
    {
        vbo_max_size = neededBufferSize + 5000;  // Grow buffer
        glBufferData(GL_ARRAY_BUFFER, vbo_max_size, NULL, GL_STREAM_DRAW);
    }

    // Copy and convert all vertices into a single contiguous buffer
    unsigned char* buffer_data = (unsigned char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!buffer_data)
        return;
    for (int n = 0; n < cmd_lists_count; n++)
    {
        const ImDrawList* cmd_list = cmd_lists[n];
        memcpy(buffer_data, &cmd_list->vtx_buffer[0], cmd_list->vtx_buffer.size() * sizeof(ImDrawVert));
        buffer_data += cmd_list->vtx_buffer.size() * sizeof(ImDrawVert);
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(vao_handle);

    int cmd_offset = 0;
    for (int n = 0; n < cmd_lists_count; n++)
    {
        const ImDrawList* cmd_list = cmd_lists[n];
        int vtx_offset = cmd_offset;
        const ImDrawCmd* pcmd_end = cmd_list->commands.end();
        for (const ImDrawCmd* pcmd = cmd_list->commands.begin(); pcmd != pcmd_end; pcmd++)
        {
            glScissor((int)pcmd->clip_rect.x, (int)(height - pcmd->clip_rect.w), (int)(pcmd->clip_rect.z - pcmd->clip_rect.x), (int)(pcmd->clip_rect.w - pcmd->clip_rect.y));
            glDrawArrays(GL_TRIANGLES, vtx_offset, pcmd->vtx_count);
            vtx_offset += pcmd->vtx_count;
        }
        cmd_offset = vtx_offset;
    }

    // Restore modified state
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_SCISSOR_TEST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static const char* ImImpl_GetClipboardTextFn()
{
    return glfwGetClipboardString(g_win.getGLFWwindow());
}

static void ImImpl_SetClipboardTextFn(const char* text)
{
    glfwSetClipboardString(g_win.getGLFWwindow(), text);
}


void InitImGui()
{
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;                                  // Time elapsed since last frame, in seconds (in this sample app we'll override this every frame because our timestep is variable)
    io.PixelCenterOffset = 0.5f;                                  // Align OpenGL texels
    io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;                       // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
    io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
    io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
    io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
    io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
    io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
    io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
    io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
    io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

    io.RenderDrawListsFn = ImImpl_RenderDrawLists;
    io.SetClipboardTextFn = ImImpl_SetClipboardTextFn;
    io.GetClipboardTextFn = ImImpl_GetClipboardTextFn;

    // Load font texture
    glGenTextures(1, &fontTex);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    const void* png_data;
    unsigned int png_size;
    ImGui::GetDefaultFontData(NULL, NULL, &png_data, &png_size);
    int tex_x, tex_y, tex_comp;
    void* tex_data = stbi_load_from_memory((const unsigned char*)png_data, (int)png_size, &tex_x, &tex_y, &tex_comp, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_x, tex_y, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data);
    stbi_image_free(tex_data);
    
    try {
        g_gui_program = loadShaders(g_GUI_file_vertex_shader, g_GUI_file_fragment_shader);
    }
    catch (std::logic_error& e) {
        std::cerr << e.what() << std::endl;
    }

    texture_location = glGetUniformLocation(g_gui_program, "Texture");
    ortho_location = glGetUniformLocation(g_gui_program, "ortho");
    position_location = glGetAttribLocation(g_gui_program, "Position");
    uv_location = glGetAttribLocation(g_gui_program, "UV");
    colour_location = glGetAttribLocation(g_gui_program, "Colour");

    glGenBuffers(1, &vbo_handle);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
    glBufferData(GL_ARRAY_BUFFER, vbo_max_size, NULL, GL_DYNAMIC_DRAW);

    glGenVertexArrays(1, &vao_handle);
    glBindVertexArray(vao_handle);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
    glEnableVertexAttribArray(position_location);
    glEnableVertexAttribArray(uv_location);
    glEnableVertexAttribArray(colour_location);

    glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(colour_location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    g_time_last = ImGui::GetTime();

}

void UpdateImGui()
{
    ImGuiIO& io = ImGui::GetIO();

    // Setup resolution (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    glfwGetWindowSize(g_win.getGLFWwindow(), &w, &h);
    glfwGetFramebufferSize(g_win.getGLFWwindow(), &display_w, &display_h);
    io.DisplaySize = ImVec2((float)display_w, (float)display_h);                                   // Display size, in pixels. For clamping windows positions.

    // Setup time step
    static double time = 0.0f;
    const double current_time = glfwGetTime();
    io.DeltaTime = (float)(current_time - time);
    time = current_time;

    // Setup inputs
    // (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
    double mouse_x, mouse_y;
    glfwGetCursorPos(g_win.getGLFWwindow(), &mouse_x, &mouse_y);
    mouse_x *= (float)display_w / w;                                                               // Convert mouse coordinates to pixels
    mouse_y *= (float)display_h / h;
    io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);                                          // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
    io.MouseDown[0] = mousePressed[0] || glfwGetMouseButton(g_win.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) != 0;  // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
    io.MouseDown[1] = mousePressed[1] || glfwGetMouseButton(g_win.getGLFWwindow(), GLFW_MOUSE_BUTTON_RIGHT) != 0;

    // Start the frame
    ImGui::NewFrame();
}


void showGUI(){
    ImGui::Begin("Volume Settings", &g_show_gui, ImVec2(300, 500));
    static float f;
    g_over_gui = ImGui::IsMouseHoveringAnyWindow();

    // Calculate and show frame rate
    static ImVector<float> ms_per_frame; if (ms_per_frame.empty()) { ms_per_frame.resize(400); memset(&ms_per_frame.front(), 0, ms_per_frame.size()*sizeof(float)); }
    static int ms_per_frame_idx = 0;
    static float ms_per_frame_accum = 0.0f;
    if (!g_pause){
        ms_per_frame_accum -= ms_per_frame[ms_per_frame_idx];
        ms_per_frame[ms_per_frame_idx] = ImGui::GetIO().DeltaTime * 1000.0f;
        ms_per_frame_accum += ms_per_frame[ms_per_frame_idx];

        ms_per_frame_idx = (ms_per_frame_idx + 1) % ms_per_frame.size();
    }
    const float ms_per_frame_avg = ms_per_frame_accum / 120;

    if (ImGui::CollapsingHeader("Input", 0, true, false))
    {

    }



    if (ImGui::CollapsingHeader("Timing"),true)
    {
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", ms_per_frame_avg, 1000.0f / ms_per_frame_avg);

        float min = *std::min_element(ms_per_frame.begin(), ms_per_frame.end());
        float max = *std::max_element(ms_per_frame.begin(), ms_per_frame.end());

        if (max - min < 10.0f){
            float mid = (max + min) * 0.5f;
            min = mid - 5.0f;
            max = mid + 5.0f;
        }

        static size_t values_offset = 0;

        char buf[50];
        sprintf(buf, "avg %f", ms_per_frame_avg);
        ImGui::PlotLines("Frame Times", &ms_per_frame.front(), (int)ms_per_frame.size(), (int)values_offset, buf, min - max * 0.1f, max * 1.1f, ImVec2(0, 70));

        ImGui::SameLine(); ImGui::Checkbox("pause", &g_pause);

        ImGui::Separator();
        ImGui::Text(std::string("Reference Dim: ").append(std::to_string(q_renderer.m_treeInfo.ref_dim.x)).append(" ").append(std::to_string(q_renderer.m_treeInfo.ref_dim.y)).c_str());
        ImGui::Text(std::string("Page Dim: ").append(std::to_string(q_renderer.m_treeInfo.page_dim.x)).append(" ").append(std::to_string(q_renderer.m_treeInfo.page_dim.y)).c_str());
        ImGui::Text(std::string("Screen Pos      : ").append(std::to_string(q_renderer.m_treeInfo.ref_pos.x)).append(" ").append(std::to_string(q_renderer.m_treeInfo.ref_pos.y)).c_str());
        ImGui::Text(std::string("Screen Pos Trans: ").append(std::to_string(q_renderer.m_treeInfo.ref_pos_trans.x)).append(" ").append(std::to_string(q_renderer.m_treeInfo.ref_pos_trans.y)).c_str());
        
        ImGui::Separator();
        ImGui::Text(std::string("max  Budget: ").append(std::to_string(q_renderer.m_treeInfo.max_budget)).c_str());
        ImGui::Text(std::string("used Budget: ").append(std::to_string(q_renderer.m_treeInfo.used_budget)).c_str());
        ImGui::Text(std::string("used ideal Budget: ").append(std::to_string(q_renderer.m_treeInfo.used_ideal_budget)).c_str());
        ImGui::Separator();
        ImGui::Text(std::string("Global Error: ").append(std::to_string(q_renderer.m_treeInfo.global_error)).c_str());
        ImGui::Separator();
        ImGui::Text(std::string("Min Prio: ").append(std::to_string(q_renderer.m_treeInfo.min_prio)).c_str());
		ImGui::SameLine();
        ImGui::Text(std::string("Max Prio: ").append(std::to_string(q_renderer.m_treeInfo.max_prio)).c_str());
        ImGui::Text(std::string("Min Importance: ").append(std::to_string(q_renderer.m_treeInfo.min_importance)).c_str());
		ImGui::SameLine();
        ImGui::Text(std::string("Max Importance: ").append(std::to_string(q_renderer.m_treeInfo.max_importance)).c_str());
		ImGui::Text(std::string("Min Error: ").append(std::to_string(q_renderer.m_treeInfo.min_error)).c_str());
		ImGui::SameLine();
		ImGui::Text(std::string("Max Error: ").append(std::to_string(q_renderer.m_treeInfo.max_error)).c_str());
        
        
        if (q_renderer.m_treeInfo.global_error_difference <= 0.0)
            ImGui::TextColored(ImVec4(0.0, 1.0, 0.0, 1.0), std::string("Global Error Difference: ").append(std::to_string(q_renderer.m_treeInfo.global_error_difference)).c_str());
        else
            ImGui::TextColored(ImVec4(1.0, 0.0, 0.0, 1.0), std::string("Global Error Difference: ").append(std::to_string(q_renderer.m_treeInfo.global_error_difference)).c_str());
        ImGui::Separator();

    }

    if (ImGui::CollapsingHeader("Window options"))
    {
        if (ImGui::TreeNode("Background Color")){
            ImGui::ColorEdit3("BC", &g_background_color[0]);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Style Editor"))
        {
            ImGui::ShowStyleEditor();
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Logging"))
        {
            ImGui::LogButtons();
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

int main(int argc, char* argv[])
{

    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL Error: " << error << std::endl;
    }


    //g_win = Window(g_window_res);
    InitImGui();


    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL Error @ Shader: " << error << std::endl;
    }


    // init object manipulator (turntable)
    Manipulator manipulator;

    // manage keys here
    // add new input if neccessary (ie changing sampling distance, isovalues, ...)
    while (!g_win.shouldClose()) {

 
        // to add key inputs:
        // check g_win.isKeyPressed(KEY_NAME)
        // - KEY_NAME - key name      (GLFW_KEY_A ... GLFW_KEY_Z)

        //if (g_win.isKeyPressed(GLFW_KEY_X)){
        //    
        //        ... do something
        //    
        //}

        if (g_win.isButtonPressed(Window::MOUSE_BUTTON_LEFT)){
            g_ref_point = g_win.mousePosition();
        }

        float pick_radius = 0.03f;

        if (g_win.isButtonPressed(Window::MOUSE_BUTTON_MIDDLE)){
            auto right_pick_point = g_win.mousePosition();

            if (glm::length(g_test_point - right_pick_point) < pick_radius){
                g_test_point = right_pick_point;
            }
            if (glm::length(g_frustrum_points[0] - right_pick_point) < pick_radius){
                g_frustrum_points[0] = right_pick_point;
            }
            else
            if (glm::length(g_frustrum_points[1] - right_pick_point) < pick_radius){
                g_frustrum_points[1] = right_pick_point;
            }
        }


        if (g_win.isButtonPressed(Window::MOUSE_BUTTON_RIGHT)){
           auto right_pick_point = g_win.mousePosition();

           if (glm::length(g_restriction_line[0] - right_pick_point) < pick_radius){
               g_restriction_line[0] = right_pick_point;
           }
           else
           if (glm::length(g_restriction_line[1] - right_pick_point) < pick_radius){
               g_restriction_line[1] = right_pick_point;
           }

        }

        q_renderer.set_restriction(g_restriction, g_restriction_line, g_restriction_direction);
        q_renderer.set_frustum(g_ref_point, g_frustrum_points);
        q_renderer.set_test_point(g_test_point);

        /// reload shader if key R ist pressed
        if (g_reload_shader){

            GLuint newProgram(0);
            try {
                //std::cout << "Reload shaders" << std::endl;
                //newProgram = loadShaders(g_file_vertex_shader, g_file_fragment_shader, g_task_chosen, g_lighting_toggle, g_shadow_toggle, g_opacity_correction_toggle);
                //g_error_message = "";
            }
            catch (std::logic_error& e) {
                //std::cerr << e.what() << std::endl;
                std::stringstream ss;
                ss << e.what() << std::endl;
                g_error_message = ss.str();
                g_reload_shader_error = true;
                newProgram = 0;
            }
            if (0 != newProgram) {
                //glDeleteProgram(g_volume_program);
                //g_volume_program = newProgram;
                //g_reload_shader_error = false;

            }
            else
            {
                g_reload_shader_error = true;

            }
        }

        if (g_win.isKeyPressed(GLFW_KEY_R)) {
            if (g_reload_shader_pressed != true) {
                g_reload_shader = true;
                g_reload_shader_pressed = true;
            }
            else{
                g_reload_shader = false;
            }
        }
        else {
            g_reload_shader = false;
            g_reload_shader_pressed = false;
        }


        GLuint error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "OpenGL Error 1370: " << error << std::endl;
        }

        glm::ivec2 size = g_win.windowSize();
        glViewport(0, 0, size.x, size.y);
        glClearColor(g_background_color.x, g_background_color.y, g_background_color.z, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float fovy = 45.0f;
        float aspect = (float)size.x / (float)size.y;
        float zNear = 0.025f, zFar = 10.0f;
        glm::mat4 projection = glm::perspective(fovy, aspect, zNear, zFar);


        glm::vec3 eye = glm::vec3(0.0f, 0.0f, 1.5f);
        glm::vec3 target = glm::vec3(0.0f);
        glm::vec3 up(0.0f, 1.0f, 0.0f);

        glm::detail::tmat4x4<float, glm::highp> view = glm::lookAt(eye, target, up);

        glm::detail::tmat4x4<float, glm::highp> turntable_matrix = manipulator.matrix();// manipulator.matrix(g_win);

        if (!g_over_gui){
            turntable_matrix = manipulator.matrix(g_win);
        }
        
        float time_current = ImGui::GetTime();
        g_time_last = time_current;
        
        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "OpenGL Error 1448: " << error << std::endl;
        }

        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        
        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "OpenGL Error 1470: " << error << std::endl;
        }

                
        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "OpenGL Error 1477: " << error << std::endl;
        }
        
        
        //IMGUI ROUTINE begin    
        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheel = 0;
        mousePressed[0] = mousePressed[1] = false;

        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "OpenGL Error 1566: " << error << std::endl;
        }

        glfwPollEvents();
        UpdateImGui();
        showGUI();

        // Rendering
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImGui::Render();
        //IMGUI ROUTINE end        
        
        q_renderer.update_and_draw(g_ref_point, glm::uvec2(io.DisplaySize.x, io.DisplaySize.y));

        glBindTexture(GL_TEXTURE_2D, 0);
        g_win.update();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

    }

    //IMGUI shutdown
    if (vao_handle) glDeleteVertexArrays(1, &vao_handle);
    if (vbo_handle) glDeleteBuffers(1, &vbo_handle);
    glDetachShader(g_gui_program, vert_handle);
    glDetachShader(g_gui_program, frag_handle);
    glDeleteShader(vert_handle);
    glDeleteShader(frag_handle);
    glDeleteProgram(g_gui_program);
    //IMGUI shutdown end

    ImGui::Shutdown();

    return 0;
}