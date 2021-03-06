#include "QuadtreeRenderer.hpp"

#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "utils.hpp"


void
QuadtreeRenderer::reload_shader(){
	auto quad_fragment_shader = readFile(quad_fragment_shader_string);
	auto quad_vertex_shader = readFile(quad_vertex_shader_string);

	auto quad_fragment_texture_shader = readFile(quad_fragment_texture_shader_string);
	auto quad_vertex_texture_shader = readFile(quad_vertex_texture_shader_string);

	m_program_id = createProgram(quad_vertex_shader, quad_fragment_shader);
	m_program_texture_id = createProgram(quad_vertex_texture_shader, quad_fragment_texture_shader);
}

QuadtreeRenderer::QuadtreeRenderer()
: m_program_id(0),
m_vao(0),
m_vao_p(0),
m_vao_r(0),
m_vbo(0),
m_vbo_p(0),
m_vbo_r(0),
m_dirty(true)
{

	reload_shader();

    m_treeInfo.min_prio = 9999.99;
    m_treeInfo.max_prio = -9999.99;

    m_tree_current = new q_tree();

    m_tree_current->budget = 2000;
    m_tree_current->budget_filled = 0;
    m_tree_current->frame_budget = 20;
    m_tree_current->max_depth = 7;

    m_tree_current->root_node = new q_node();
    m_tree_current->root_node->node_id = 0;
    m_tree_current->root_node->leaf = true;
    m_tree_current->root_node->depth = 0;
    m_tree_current->root_node->priority = 0.0;
    m_tree_current->root_node->error = 0.0;

    m_tree_current->root_node->tree = m_tree_current;
	    
    size_t max_nodes_finest_level = q_layout.total_node_count_level(m_tree_current->max_depth);
    size_t total_nodes = q_layout.total_node_count(m_tree_current->max_depth);

    std::cout << "max_nodes_finest_level " << max_nodes_finest_level << " Dimx_y: " << (size_t)glm::sqrt((float)max_nodes_finest_level) << " Total nodes:" << total_nodes << std::endl;

    m_tree_resolution = (unsigned)glm::sqrt((float)max_nodes_finest_level);

    m_tree_current->qtree_index_data.resize(max_nodes_finest_level, m_tree_current->root_node);
    m_tree_current->qtree_depth_data.resize(max_nodes_finest_level, 0);
    m_tree_current->qtree_id_data.resize(max_nodes_finest_level, 0);
    m_tree_current->qtree_importance_data.resize(max_nodes_finest_level, 0);

    m_treeInfo.max_budget = m_tree_current->budget;
    m_treeInfo.used_budget = 0;

    m_treeInfo.page_dim = glm::uvec2(256, 256);
    m_treeInfo.ref_dim = glm::uvec2(2560, 2560);

    m_treeInfo.global_error = (float)m_treeInfo.ref_dim.x / m_treeInfo.page_dim.x + (float)m_treeInfo.ref_dim.y / m_treeInfo.page_dim.y;
    m_treeInfo.global_error_difference = (float)m_treeInfo.ref_dim.x / m_treeInfo.page_dim.x + (float)m_treeInfo.ref_dim.y / m_treeInfo.page_dim.y;

    m_quadVertices.clear();
    m_quadVertices.push_back(glm::vec3(0.0, 0.0, 0.0));
    m_quadVertices.push_back(glm::vec3(0.0, 1.0, 0.0));
    m_quadVertices.push_back(glm::vec3(1.0, 0.0, 0.0));
    m_quadVertices.push_back(glm::vec3(1.0, 1.0, 0.0));

	///////////////////////////////////////////////
	auto n = m_tree_current->root_node;
	split_node(n);	
	n = n->child_node[0];
	split_node(n);
	n = n->child_node[0];
	split_node(n);
	n = n->child_node[0];
	split_node(n);

	///////////////////////////////////////////////

    glGenVertexArrays(1, &m_vao_quad);
    glBindVertexArray(m_vao_quad);

    glGenBuffers(1, &m_vbo_quad);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_quad);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * m_quadVertices.size()
        , m_quadVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    //glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
    glBindVertexArray(0);


#if 0
    glActiveTexture(GL_TEXTURE0);
    m_texture_id_current = createTexture2D(m_tree_resolution, m_tree_resolution, (char*)&m_tree_current->qtree_depth_data[0], GL_R8, GL_RED, GL_UNSIGNED_BYTE);
#elif 0
    glActiveTexture(GL_TEXTURE0);
    m_texture_id_current = createTexture2D(m_tree_resolution, m_tree_resolution, (char*)&m_tree_current->qtree_id_data[0], GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);
#elif 1
    glActiveTexture(GL_TEXTURE0);
    m_texture_id_current = createTexture2D(m_tree_resolution, m_tree_resolution, (char*)&m_tree_current->qtree_importance_data[0], GL_R32F, GL_RED, GL_FLOAT);
#endif

}

void
QuadtreeRenderer::init_tree(QuadtreeRenderer::q_tree_ptr tree){
    tree = new q_tree();

    tree->budget = m_tree_current->budget;
    tree->budget_filled = 0;
    tree->frame_budget = 9999999;
    tree->max_depth = m_tree_current->max_depth;

    tree->root_node = new q_node();
    tree->root_node->node_id = 0;
    tree->root_node->leaf = true;
    tree->root_node->depth = 0;
    tree->root_node->priority = 0.0;
    tree->root_node->error = 0.0;//projection of face to POI/Camera

    tree->root_node->tree = tree;

    size_t max_nodes_finest_level = q_layout.total_node_count_level(m_tree_current->max_depth);
    size_t total_nodes = q_layout.total_node_count(m_tree_current->max_depth);

    //std::cout << "max_nodes_finest_level " << max_nodes_finest_level << " Dimx_y: " << (size_t)glm::sqrt((float)max_nodes_finest_level) << " Total nodes:" << total_nodes << std::endl;

    m_tree_resolution = (unsigned)glm::sqrt((float)max_nodes_finest_level);

    tree->qtree_index_data.resize(max_nodes_finest_level, tree->root_node);

    tree->qtree_depth_data.resize(max_nodes_finest_level, 0);

    tree->qtree_id_data.resize(max_nodes_finest_level, 0);

    tree->qtree_importance_data.resize(max_nodes_finest_level, 0);
}

void
QuadtreeRenderer::set_restriction(bool restriction, glm::vec2 restriction_line[2], bool restriction_direction)
{
    m_restriction = restriction;
    m_restriction_line[0] = restriction_line[0];
    m_restriction_line[1] = restriction_line[1];

    m_restriction_direction = restriction_direction;
}

void
QuadtreeRenderer::set_frustum(const unsigned frust_nbr, glm::vec2 camera, glm::vec2 frustum_points[2])
{
	if (m_frustrum_2d_vec.size() < (frust_nbr + 1))
		m_frustrum_2d_vec.push_back(frustrum_2d());

	m_frustrum_2d_vec[frust_nbr].m_camera_point = camera;
	m_frustrum_2d_vec[frust_nbr].m_frustrum_points[0] = frustum_points[0];
	m_frustrum_2d_vec[frust_nbr].m_frustrum_points[1] = frustum_points[1];

}

void
QuadtreeRenderer::set_splits_per_frame(const int splits_per_frame)
{
	m_tree_current->frame_budget = splits_per_frame;
}

void
QuadtreeRenderer::set_test_point(glm::vec2 test_point)
{
    m_test_point = test_point;
}

bool
QuadtreeRenderer::check_frustrum(QuadtreeRenderer::q_node_ptr n) const
{
    auto pos = q_layout.node_position(n->node_id);
    auto node_level = q_layout.level_index(n->node_id);
    auto total_node_index = q_layout.total_node_count(node_level);

    auto max_pos = q_layout.node_position(total_node_index - 1);

    auto v_pos = glm::vec2((float)pos.x / (max_pos.x + 1.0), (float)pos.y / (max_pos.y + 1.0));
    auto v_length = 1.0 / (max_pos.x + 1);

    glm::vec2 vpos1 = v_pos;
    glm::vec2 vpos2 = glm::vec2(v_pos.x + v_length, v_pos.y);
    glm::vec2 vpos3 = glm::vec2(v_pos.x + v_length, v_pos.y + v_length);
    glm::vec2 vpos4 = glm::vec2(v_pos.x, v_pos.y + v_length);

    glm::vec4 trans = m_model * glm::vec4(vpos1, 0.0f, 1.0f);
    glm::vec2 trans1 = glm::vec2(trans.x, trans.y);

    trans = m_model * glm::vec4(vpos2, 0.0f, 1.0f);
    glm::vec2 trans2 = glm::vec2(trans.x, trans.y);

    trans = m_model * glm::vec4(vpos3, 0.0f, 1.0f);
    glm::vec2 trans3 = glm::vec2(trans.x, trans.y);

    trans = m_model * glm::vec4(vpos4, 0.0f, 1.0f);
    glm::vec2 trans4 = glm::vec2(trans.x, trans.y);

	bool in_frustrum = false;

	unsigned frust_nbr = 0;

	for(auto f : m_frustrum_2d_vec){
		glm::vec2 f1 = f.m_frustrum_points[0];
		glm::vec2 f2 = f.m_frustrum_points[1];

		glm::vec2 p1 = glm::vec2(trans1.x, trans1.y);
		glm::vec2 p2 = glm::vec2(trans2.x, trans2.y);
		glm::vec2 p3 = glm::vec2(trans3.x, trans3.y);
		glm::vec2 p4 = glm::vec2(trans4.x, trans4.y);

		glm::vec2 cp = f.m_camera_point;
		glm::vec2 rp1;
		glm::vec2 rp2;
	
		auto ipoint1 = intersect2D_2Segments(cp, f1, p1, p2, &rp1, &rp2);
		auto ipoint2 = intersect2D_2Segments(cp, f1, p2, p3, &rp1, &rp2);
		auto ipoint3 = intersect2D_2Segments(cp, f1, p3, p4, &rp1, &rp2);
		auto ipoint4 = intersect2D_2Segments(cp, f1, p4, p1, &rp1, &rp2);
		auto ipoint5 = intersect2D_2Segments(cp, f2, p1, p2, &rp1, &rp2);
		auto ipoint6 = intersect2D_2Segments(cp, f2, p2, p3, &rp1, &rp2);
		auto ipoint7 = intersect2D_2Segments(cp, f2, p3, p4, &rp1, &rp2);
		auto ipoint8 = intersect2D_2Segments(cp, f2, p4, p1, &rp1, &rp2);
	
		if (!(
			check_frustrum(frust_nbr, trans1)
			|| check_frustrum(frust_nbr, trans2)
			|| check_frustrum(frust_nbr, trans3)
			|| check_frustrum(frust_nbr, trans4)
			|| ipoint1 != 0
			|| ipoint2 != 0
			|| ipoint3 != 0
			|| ipoint4 != 0
			|| ipoint5 != 0
			|| ipoint6 != 0
			|| ipoint7 != 0
			|| ipoint8 != 0
			)){

			//in_frustrum = false;
		}
		else {
			in_frustrum = true;
		}

		++frust_nbr;
	}
    return in_frustrum;
}


bool
QuadtreeRenderer::check_frustrum(const unsigned frust_nbr, QuadtreeRenderer::q_node_ptr n) const
{
	auto pos = q_layout.node_position(n->node_id);
	auto node_level = q_layout.level_index(n->node_id);
	auto total_node_index = q_layout.total_node_count(node_level);

	auto max_pos = q_layout.node_position(total_node_index - 1);

	auto v_pos = glm::vec2((float)pos.x / (max_pos.x + 1.0), (float)pos.y / (max_pos.y + 1.0));
	auto v_length = 1.0 / (max_pos.x + 1);

	glm::vec2 vpos1 = v_pos;
	glm::vec2 vpos2 = glm::vec2(v_pos.x + v_length, v_pos.y);
	glm::vec2 vpos3 = glm::vec2(v_pos.x + v_length, v_pos.y + v_length);
	glm::vec2 vpos4 = glm::vec2(v_pos.x, v_pos.y + v_length);

	glm::vec4 trans = m_model * glm::vec4(vpos1, 0.0f, 1.0f);
	glm::vec2 trans1 = glm::vec2(trans.x, trans.y);

	trans = m_model * glm::vec4(vpos2, 0.0f, 1.0f);
	glm::vec2 trans2 = glm::vec2(trans.x, trans.y);

	trans = m_model * glm::vec4(vpos3, 0.0f, 1.0f);
	glm::vec2 trans3 = glm::vec2(trans.x, trans.y);

	trans = m_model * glm::vec4(vpos4, 0.0f, 1.0f);
	glm::vec2 trans4 = glm::vec2(trans.x, trans.y);

	bool in_frustrum = false;
	
	auto& f = m_frustrum_2d_vec[frust_nbr];

	glm::vec2 f1 = f.m_frustrum_points[0];
	glm::vec2 f2 = f.m_frustrum_points[1];

	glm::vec2 p1 = glm::vec2(trans1.x, trans1.y);
	glm::vec2 p2 = glm::vec2(trans2.x, trans2.y);
	glm::vec2 p3 = glm::vec2(trans3.x, trans3.y);
	glm::vec2 p4 = glm::vec2(trans4.x, trans4.y);

	glm::vec2 cp = f.m_camera_point;
	glm::vec2 rp1;
	glm::vec2 rp2;

	auto ipoint1 = intersect2D_2Segments(cp, f1, p1, p2, &rp1, &rp2);
	auto ipoint2 = intersect2D_2Segments(cp, f1, p2, p3, &rp1, &rp2);
	auto ipoint3 = intersect2D_2Segments(cp, f1, p3, p4, &rp1, &rp2);
	auto ipoint4 = intersect2D_2Segments(cp, f1, p4, p1, &rp1, &rp2);
	auto ipoint5 = intersect2D_2Segments(cp, f2, p1, p2, &rp1, &rp2);
	auto ipoint6 = intersect2D_2Segments(cp, f2, p2, p3, &rp1, &rp2);
	auto ipoint7 = intersect2D_2Segments(cp, f2, p3, p4, &rp1, &rp2);
	auto ipoint8 = intersect2D_2Segments(cp, f2, p4, p1, &rp1, &rp2);

	if (!(
		check_frustrum(frust_nbr, trans1)
		|| check_frustrum(frust_nbr, trans2)
		|| check_frustrum(frust_nbr, trans3)
		|| check_frustrum(frust_nbr, trans4)
		|| ipoint1 != 0
		|| ipoint2 != 0
		|| ipoint3 != 0
		|| ipoint4 != 0
		|| ipoint5 != 0
		|| ipoint6 != 0
		|| ipoint7 != 0
		|| ipoint8 != 0
		)) {

		//in_frustrum = false;
	}
	else {
		in_frustrum = true;
	}

	return in_frustrum;
}


bool
QuadtreeRenderer::check_frustrum(const unsigned frust_nbr, glm::vec2 pos) const
{
    glm::vec2 p1 = m_frustrum_2d_vec[frust_nbr].m_frustrum_points[0];
    glm::vec2 p2 = m_frustrum_2d_vec[frust_nbr].m_frustrum_points[1];

    glm::vec2 c = m_frustrum_2d_vec[frust_nbr].m_camera_point;

	p1 = p1 + ((p1 - c) * 100.0f);
	p2 = p2 + ((p2 - c) * 100.0f);

    glm::vec2 min = glm::vec2(glm::min(p1.x, p2.x), glm::min(p1.y, p2.y));
    glm::vec2 max = glm::vec2(glm::max(p1.x, p2.x), glm::max(p1.y, p2.y));

    glm::vec2 n1 = c - p1;
    n1 = glm::vec2(-n1.y, n1.x);

    glm::vec2 n2 = p2 - c;
    n2 = glm::vec2(-n2.y, n2.x);

    float s1 = glm::dot(n1, pos - c);
    float s2 = glm::dot(n2, pos - c);

    //std::cout << "d1: " << s1 << " d2: " << s2 << std::endl;

    if (s1 > 0.0 && s2 > 0.0)
        return true;
    else
        return false;

    //http://upload.wikimedia.org/math/c/a/e/cae82210ba48deb8ff6fb0134a90b36e.png

}


void
QuadtreeRenderer::split_node(QuadtreeRenderer::q_node_ptr n)
{
    for (unsigned c = 0; c != CHILDREN; ++c){
        n->child_node[c] = new q_node();
        n->child_node[c]->parent = n;
        n->child_node[c]->node_id = q_layout.child_node_index(n->node_id, c);
        n->child_node[c]->leaf = true;
        n->child_node[c]->depth = n->depth + 1;

        n->child_node[c]->tree = n->tree;

        n->child_node[c]->error = get_error_of_node(n->child_node[c]);
        n->child_node[c]->importance = get_importance_of_node(n->child_node[c]);
       
		auto prio = 0.0f;

		if (n->child_node[c]->error < 0.0) {
			prio = n->child_node[c]->importance + n->child_node[c]->error;
		}
		else {
			prio = n->child_node[c]->importance * n->child_node[c]->error;
		}

		n->child_node[c]->priority = prio;

        ///////setting node index image
        size_t max_nodes_finest_level = q_layout.total_node_count_level(n->tree->max_depth);
        auto nodes_on_lvl = q_layout.total_node_count_level(n->child_node[c]->depth);
        auto one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
        auto node_pos = q_layout.node_position(n->child_node[c]->node_id);
        auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);

        for (unsigned y = 0; y != one_node_to_finest; ++y){
            for (unsigned x = 0; x != one_node_to_finest; ++x){
                size_t index = (node_pos.x * one_node_to_finest + x) + (resolution - 1 - ((node_pos.y) * one_node_to_finest + y)) * resolution;
                n->tree->qtree_index_data[index] = n->child_node[c];
                n->tree->qtree_depth_data[index] = n->child_node[c]->depth;
                n->tree->qtree_importance_data[index] = n->child_node[c]->importance;
                n->tree->qtree_id_data[index] = n->child_node[c]->node_id;
            }
        }
    }

    n->tree->budget_filled += CHILDREN;
#if 0
    size_t max_nodes_finest_level = q_layout.total_node_count_level(n->tree->max_depth);
    auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);

    for (unsigned y = 0; y != resolution; ++y){
        for (unsigned x = 0; x != resolution; ++x){
            auto index = (x) + (y) * resolution;
            if (n->tree->qtree_index_data[index]->node_id < 10)
                std::cout << " " << n->tree->qtree_index_data[index]->node_id << " ";
            else
                std::cout << n->tree->qtree_index_data[index]->node_id << " ";
        }
        std::cout<<std::endl;
    }
    std::cout << std::endl;

#endif
    n->leaf = false;
}

bool
QuadtreeRenderer::splitable(QuadtreeRenderer::q_node_ptr n) const{

    if (n->depth >= n->tree->max_depth){
        return false;
    }

    if (n->valid == false)
        return false;

    for (unsigned c = 0; c != CHILDREN; ++c){
        if (n->child_node[c] != nullptr && n->child_node[c]->valid != false){
            return false;
        }
    }
    return true;
}

bool
QuadtreeRenderer::collabsible(QuadtreeRenderer::q_node_ptr n) const{

    if (n == nullptr || n->valid == false){
        return false;
    }

    if (n->leaf){
        return false;
    }

	if (n->dependend_mark) {
		return false;
	}

    for (unsigned c = 0; c != CHILDREN; ++c){
        if (n->child_node[c] != nullptr 
            && n->child_node[c]->valid == true 
            && n->child_node[c]->leaf != true
            || n->child_node[c]->dependend_mark == true){
            return false;
        }
    }
    return true;
}


void
QuadtreeRenderer::collapse_node(QuadtreeRenderer::q_node_ptr n)
{
    assert(!n->leaf);

    for (unsigned c = 0; c != CHILDREN; ++c){

        //delete n->child_node[c];        
        n->child_node[c]->valid = false;
        cleanup_container.push_back(n->child_node[c]);
        n->child_node[c] = nullptr;
    }

    n->error = get_error_of_node(n);
    n->importance = get_importance_of_node(n);
    //n->priority = n->importance * n->error;

	auto prio = 0.0f;

	if (n->error < 0.0) {
		prio = n->importance + n->error;;
	}
	else {
		prio = n->importance * n->error;
	}

	n->priority = prio;

    n->tree->budget_filled -= CHILDREN;

    n->leaf = true;

    ///////setting node index image
    size_t max_nodes_finest_level = q_layout.total_node_count_level(n->tree->max_depth);
    auto nodes_on_lvl = q_layout.total_node_count_level(n->depth);
    auto one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
    auto node_pos = q_layout.node_position(n->node_id);
    auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);

    for (unsigned y = 0; y != one_node_to_finest; ++y){
        for (unsigned x = 0; x != one_node_to_finest; ++x){
            size_t index = (node_pos.x * one_node_to_finest + x) + (resolution - 1 - ((node_pos.y) * one_node_to_finest + y)) * resolution;
            n->tree->qtree_index_data[index] = n;
            n->tree->qtree_depth_data[index] = n->depth;
            n->tree->qtree_importance_data[index] = n->importance;
            n->tree->qtree_id_data[index] = n->node_id;
        }
    }
}

QuadtreeRenderer::q_node_ptr
QuadtreeRenderer::get_neighbor_node(const QuadtreeRenderer::q_node_ptr n, const QuadtreeRenderer::q_tree_ptr tree, const unsigned neighbor_nbr) const
{

    size_t max_nodes_finest_level = q_layout.total_node_count_level(n->tree->max_depth);
    auto nodes_on_lvl = q_layout.total_node_count_level(n->depth);
    unsigned one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
    auto node_pos = q_layout.node_position(n->node_id);
    auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);


    auto offset_x = -1;
    auto offset_y = -1;

    switch (neighbor_nbr){
    case 0:
        offset_x = -1;
        offset_y = -1;
        break;
    case 1:
        offset_x = 0;
        offset_y = -1;
        break;
    case 2:
        offset_x = one_node_to_finest;
        offset_y = -1;
        break;
    case 3:
        offset_x = one_node_to_finest;
        offset_y = 0;
        break;
    case 4:
        offset_x = one_node_to_finest;
        offset_y = one_node_to_finest;
        break;
    case 5:
        offset_x = 0;
        offset_y = one_node_to_finest;
        break;
    case 6:
        offset_x = -1;
        offset_y = one_node_to_finest;
        break;
    case 7:
        offset_x = -1;
        offset_y = 0;
        break;
    case 8:
        offset_x = one_node_to_finest / 2;
        offset_y = -1;
        break;
    case 9:
        offset_x = one_node_to_finest;
        offset_y = one_node_to_finest / 2;
        break;
    case 10:
        offset_x = one_node_to_finest / 2;
        offset_y = one_node_to_finest;
        break;
    case 11:
        offset_x = -1;
        offset_y = one_node_to_finest / 2;
        break;
    default:
        break;
    }

    auto posx = node_pos.x * one_node_to_finest;
    auto posy = node_pos.y * one_node_to_finest;

    if ((posx + offset_x) < 0 ||
        (posy + offset_y) < 0 ||
        (posx + offset_x) >= resolution ||
        (posy + offset_y) >= resolution)
    {
        return nullptr;
    }

    //size_t index = (posx + x) + (resolution - 1 - (posy + y)) * resolution;
    size_t index = (posx + offset_x) + (resolution - 1 - (posy + offset_y)) * resolution;

    //if (tree->qtree_index_data[index])
    //    std::cout << "Neighbor: " << neighbor_nbr << " Pos: " << posx + offset_x << " , " << node_pos.y * one_node_to_finest - offset_y << " d: " << n->depth << " dn: " << tree->qtree_index_data[index]->depth << " ID: " << n->node_id << std::endl;

    if (tree->qtree_index_data[index] == n){
        std::cout << "Neighbor Node is Node itself - Error!" << std::endl;
        assert(0);
    }

    return tree->qtree_index_data[index];
}

std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::check_neighbors_for_level_div(const QuadtreeRenderer::q_node_ptr n, const float lvl_diff) const
{
    //std::cout << "!" << std::endl;

    assert(n);

    std::vector<q_node_ptr> p_nodes;

    for (unsigned n_nbr = 0; n_nbr != NEIGHBORS; ++n_nbr){
        auto neighbor = get_neighbor_node(n, n->tree, n_nbr);

        if (neighbor)
        if (((int)n->depth - (int)neighbor->depth) > lvl_diff){
            p_nodes.push_back(neighbor);
        }
    }

    return p_nodes;
}


std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::check_neighbors_for_split(const QuadtreeRenderer::q_node_ptr n) const
{
    //std::cout << "!" << std::endl;

    assert(n);

    std::vector<q_node_ptr> p_nodes;

    for (unsigned n_nbr = 0; n_nbr != NEIGHBORS; ++n_nbr){
        auto neighbor = get_neighbor_node(n, n->tree, n_nbr);

        if (neighbor){
            if (((int)n->depth - (int)neighbor->depth) >= 1.0){
                p_nodes.push_back(neighbor);
            }
        }
    }

    return p_nodes;
}

std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::check_and_mark_neighbors_for_split(const QuadtreeRenderer::q_node_ptr n)
{
    assert(n);

    std::vector<q_node_ptr> p_nodes;
	std::set<q_node_ptr> p_nodes_set;

    for (unsigned n_nbr = 0; n_nbr != NEIGHBORS; ++n_nbr) {
        auto neighbor = get_neighbor_node(n, n->tree, n_nbr);

        if (neighbor) {
                        
            if (((int)n->depth - (int)neighbor->depth) >= 1.0) {
                //p_nodes.push_back(neighbor);
				p_nodes_set.insert(neighbor);
                //neighbor->dependend_mark = true;
            }
			else {
				neighbor->parent->dependend_mark = true;
			}

            /*if (((int)n->depth - (int)neighbor->depth) >= 0.0) {
                neighbor->dependend_mark = true;
            }*/
        }
    }

	for (auto n : p_nodes_set)
		p_nodes.push_back(n);

    return p_nodes;
}

std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::check_neighbors_for_collapse(const QuadtreeRenderer::q_node_ptr n) const
{
    //std::cout << "!" << std::endl;

    assert(n);
    assert(!n->leaf);

    std::vector<q_node_ptr> p_nodes;

    for (unsigned n_nbr = 0; n_nbr != (NEIGHBORS + NEIGHBORS / 2); ++n_nbr){
        auto neighbor = get_neighbor_node(n, n->tree, n_nbr);

        if (neighbor)
        if (((int)neighbor->depth) - (int)n->depth == 2){
            p_nodes.push_back(neighbor);
			n->checked_mark = true;
        }
    }

    return p_nodes;
}

std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::check_neighbors_for_restricted(const QuadtreeRenderer::q_node_ptr n) const
{
    //std::cout << "!" << std::endl;

    assert(n);

    std::vector<q_node_ptr> p_nodes;

    for (unsigned n_nbr = 0; n_nbr != NEIGHBORS; ++n_nbr){
        auto neighbor = get_neighbor_node(n, n->tree, n_nbr);

        if (neighbor)
        if (((int)n->depth - (int)neighbor->depth) >= 2.0){
            p_nodes.push_back(neighbor);
        }
    }

    return p_nodes;
}

float
QuadtreeRenderer::get_importance_of_node(q_node_ptr n) const
{

    auto pos = glm::vec2(q_layout.node_position(n->node_id)) + glm::vec2(0.5);
    auto node_level = q_layout.level_index(n->node_id);
    auto total_node_index = q_layout.total_node_count(node_level);

    size_t max_nodes_finest_level = q_layout.total_node_count_level(n->depth);
    auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);

	double l = 1000000.0;
	
	unsigned frust_nbr = 0;
	
	for (auto& f : m_frustrum_2d_vec) {

		if (check_frustrum(frust_nbr, n)) {
			auto camera_pos = (float)resolution * glm::vec2(f.m_camera_point_trans.x, f.m_camera_point_trans.y);

			l = std::min(glm::length(glm::vec2(pos.x, pos.y) - camera_pos) + 1.0, l);
		}

		++frust_nbr;
	}
    auto importance = 1.0f / (l * l);

    //    return 1.0f;
    return importance;

}

float
QuadtreeRenderer::get_error_of_node(q_node_ptr n) const
{

    auto pos = q_layout.node_position(n->node_id);
    auto node_level = q_layout.level_index(n->node_id);
    auto total_node_index = q_layout.total_node_count(node_level);

    auto max_pos = q_layout.node_position(total_node_index - 1);

    auto v_pos = glm::vec2((float)pos.x / (max_pos.x + 1.0), (float)pos.y / (max_pos.y + 1.0));
    auto v_length = 1.0 / (max_pos.x + 1.0);

    glm::vec2 vpos1 = v_pos;
    glm::vec2 vpos2 = glm::vec2(v_pos.x + v_length, v_pos.y);
    glm::vec2 vpos3 = glm::vec2(v_pos.x + v_length, v_pos.y + v_length);
    glm::vec2 vpos4 = glm::vec2(v_pos.x, v_pos.y + v_length);

	glm::vec4 trans;
	
	trans = m_model * glm::vec4(vpos1, 0.0f, 1.0f);
    glm::vec2 trans1 = glm::vec2(trans.x, trans.y);

    trans = m_model * glm::vec4(vpos2, 0.0f, 1.0f);
    glm::vec2 trans2 = glm::vec2(trans.x, trans.y);

    trans = m_model * glm::vec4(vpos3, 0.0f, 1.0f);
    glm::vec2 trans3 = glm::vec2(trans.x, trans.y);

    trans = m_model * glm::vec4(vpos4, 0.0f, 1.0f);
    glm::vec2 trans4 = glm::vec2(trans.x, trans.y);

    float error = 0.0;

    //if (!(check_frustrum(trans1)
    //    || check_frustrum(trans2)
    //    || check_frustrum(trans3)
    //    || check_frustrum(trans4)
    //    )){

	if (!(check_frustrum(n)))
	{
        error = -1.0 * n->depth;
    }
    else
    {
        auto depth = n->depth + 1;
				
		//auto local_error = ((float)m_treeInfo.ref_dim.x / (m_treeInfo.page_dim.x * std::sqrt(depth)) + (float)m_treeInfo.ref_dim.y / (m_treeInfo.page_dim.y * std::sqrt(depth))) * 0.5;
		auto local_error = 1.0 / ((m_treeInfo.page_dim.x * m_treeInfo.page_dim.y * std::powf((float)CHILDREN, (float)depth)) / ((m_treeInfo.page_dim.x * m_treeInfo.page_dim.y * std::powf((float)CHILDREN, (float)n->tree->max_depth))));

        error = local_error;
    }
    //return 1.0f;
    return error;

}

void
QuadtreeRenderer::resolve_dependencies_priorities(QuadtreeRenderer::q_node_ptr node, int& counter) {
    
    if (!splitable(node))
        return;

    float eps = 0.01;
    
    auto node_dependencies = check_and_mark_neighbors_for_split(node);	
	std::sort(node_dependencies.begin(), node_dependencies.end(), less_than_priority());

	//std::stack<q_node_ptr> node_stack;

    for (auto& n : node_dependencies) {
        n->priority = node->priority + eps;
		n->dependend_mark = true;
        //node_stack.push(n);
    }
	
    while (!node_dependencies.empty() && counter != m_tree_current->frame_budget) {

        auto current_node = *(node_dependencies.end() - 1);
        node_dependencies.erase(node_dependencies.end() - 1);

        resolve_dependencies_priorities(current_node, counter);

		std::sort(node_dependencies.begin(), node_dependencies.end(), less_than_priority());

    }

/*    if (node_dependencies.empty()) {
        ++counter;
    } */   
}
    
void
QuadtreeRenderer::update_priorities(QuadtreeRenderer::q_tree_ptr tree){

    std::stack<q_node_ptr> node_stack;

    node_stack.push(tree->root_node);

    q_node_ptr current_node;

    std::vector<q_node_ptr> leafs = get_leaf_nodes(tree);

    for (auto& l = leafs.begin(); l != leafs.end(); ++l){
		
        (*l)->importance = get_importance_of_node(*l);
        (*l)->error = get_error_of_node(*l);
				
		auto prio = 0.0f;
		
		if ((*l)->error < 0.0) {
			prio = (*l)->importance + (*l)->error;;
		}
		else {
			prio = (*l)->importance * (*l)->error;
		}
		
		(*l)->priority = prio;

    }

    auto global_error = 0.0;
    for (auto& l = leafs.begin(); l != leafs.end(); ++l){
        global_error += ((1.0 / (leafs.size() * m_treeInfo.page_dim.x * m_treeInfo.page_dim.y)) * ((*l)->error) * (*l)->importance) * 10000.0;
    }

    m_treeInfo.global_error_difference = global_error - m_treeInfo.global_error;
    m_treeInfo.global_error = global_error;


    //resolve dependencies
    std::vector<q_node_ptr> split_able_nodes = get_splitable_nodes(tree);
    std::sort(split_able_nodes.begin(), split_able_nodes.end(), less_than_priority());
    //std::priority_queue<q_node_ptr, std::vector<q_node_ptr>, lesser_prio_ptr> split_able_nodes_pq(split_able_nodes.begin(), split_able_nodes.end());

   int split_counter = 0;

    while (!split_able_nodes.empty() && split_counter != m_tree_current->frame_budget) {        
        auto cur_top_node = *(split_able_nodes.end() - 1);
        resolve_dependencies_priorities(cur_top_node, split_counter);   
		cur_top_node->split_mark = true;
		split_able_nodes.erase(split_able_nodes.end() - 1);

		std::sort(split_able_nodes.begin(), split_able_nodes.end(), less_than_priority());
		++split_counter;
	}

    //if (split_counter != m_tree_current->frame_budget) {
    //    std::cout << "Went through all" << std::endl;
    //}

    for (auto& l = leafs.begin(); l != leafs.end(); ++l) {
        if((*l)->parent)
            (*l)->parent->priority = - 999999.0;
    }

	for (auto& l = leafs.begin(); l != leafs.end(); ++l) {
		//if ((*l)->parent)
		//	(*l)->parent->priority = std::max((*l)->priority, (*l)->parent->priority);
		if ((*l)->parent) {
			auto parent = (*l)->parent;
			parent->importance = get_importance_of_node(parent);
			parent->error = get_error_of_node(parent);

			auto prio = 0.0f;

			if ((parent)->error < 0.0) {
				prio = (parent)->importance + (parent)->error;;
			}
			else {
				prio = (parent)->importance * (parent)->error;
			}

			(parent)->priority = prio;
		}
	}

}

void
QuadtreeRenderer::clear_tree_marks(QuadtreeRenderer::q_tree_ptr tree) {

    std::stack<q_node_ptr> node_stack;

    node_stack.push(tree->root_node);

    q_node_ptr current_node;

    std::vector<q_node_ptr> leafs;

    while (!node_stack.empty()) {
        current_node = node_stack.top();
        node_stack.pop();

        //reset dependend mark for each node before everything else
        current_node->dependend_mark = false;
		current_node->split_mark = false;
		current_node->checked_mark = false;

        if (!current_node->leaf) {
            for (unsigned c = 0; c != CHILDREN; ++c) {
                if (current_node->child_node[c]) {
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
    }
}


void
QuadtreeRenderer::update_importance_map(QuadtreeRenderer::q_tree_ptr tree) {

    auto leafs = get_leaf_nodes(tree);

    for (auto& n : leafs) {
        /////insert into map
        size_t max_nodes_finest_level = q_layout.total_node_count_level(tree->max_depth);
        auto nodes_on_lvl = q_layout.total_node_count_level(n->depth);
        auto one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
        auto node_pos = q_layout.node_position(n->node_id);
        auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);

        for (unsigned y = 0; y != one_node_to_finest; ++y) {
            for (unsigned x = 0; x != one_node_to_finest; ++x) {
                size_t index = (node_pos.x * one_node_to_finest + x) + (resolution - 1 - ((node_pos.y) * one_node_to_finest + y)) * resolution;
				//tree->qtree_importance_data[index] = n->importance;
				//tree->qtree_importance_data[index] = n->error;
				tree->qtree_importance_data[index] = n->priority;
            }
        }
        ////////////////////
    }
       
}


void
QuadtreeRenderer::set_max_neigbor_priorities(QuadtreeRenderer::q_tree_ptr tree){


    for (auto& d = tree->max_depth; d != 0; --d){
        auto leaf_nodes = get_leaf_nodes_with_depth_outside(tree, d);

        for (auto& n : leaf_nodes){
            //set_priority_to_neighbor_max(n);
            for (unsigned n_nbr = 0; n_nbr != (NEIGHBORS + NEIGHBORS / 2); ++n_nbr){
                auto neighbor = get_neighbor_node(n, n->tree, n_nbr);

                if (neighbor)
                {
                    //n->priority = glm::max(n->priority, neighbor->priority);
                }
            }
        }
    }

}

std::map<unsigned, QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::get_all_current_leafs(QuadtreeRenderer::q_tree_ptr tree) const
{
    std::stack<q_node_ptr> node_stack;
    node_stack.push(tree->root_node);

    std::map<unsigned, q_node_ptr> leaf_node_map;
    q_node_ptr current_node;

    while (!node_stack.empty()){
        current_node = node_stack.top();
        node_stack.pop();

        if (!current_node->leaf){
            for (unsigned c = 0; c != CHILDREN; ++c){
                if (current_node->child_node[c]){
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
        else{
            leaf_node_map.insert(std::pair<unsigned, q_node_ptr>(current_node->node_id, current_node));
        }
    }

    return leaf_node_map;
}

std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::get_splitable_nodes(QuadtreeRenderer::q_tree_ptr t) const
{
    std::vector<q_node_ptr> split_able_nodes;
    std::stack<q_node_ptr> node_stack;
    node_stack.push(t->root_node);

    q_node_ptr current_node;

    while (!node_stack.empty()){
        current_node = node_stack.top();
        node_stack.pop();

        if (!current_node->leaf){
            for (unsigned c = 0; c != CHILDREN; ++c){
                if (current_node->child_node[c]){
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
        else{
            if (splitable(current_node))
                split_able_nodes.push_back(current_node);
        }
    }
    return split_able_nodes;
}

std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::get_collabsible_nodes(QuadtreeRenderer::q_tree_ptr t) const
{
    std::vector<q_node_ptr> colap_able_nodes;
    std::stack<q_node_ptr> node_stack;
    node_stack.push(t->root_node);

    q_node_ptr current_node;

    while (!node_stack.empty()){
        current_node = node_stack.top();
        node_stack.pop();

        if (!collabsible(current_node)){
            for (unsigned c = 0; c != CHILDREN; ++c){
                if (current_node->child_node[c]){
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
        else{
            colap_able_nodes.push_back(current_node);
        }
    }
    return colap_able_nodes;
}


void
QuadtreeRenderer::set_collabsible_nodes_priorities(QuadtreeRenderer::q_tree_ptr t)
{
    std::stack<q_node_ptr> node_stack;
    node_stack.push(t->root_node);

    q_node_ptr current_node;

    while (!node_stack.empty()) {
        current_node = node_stack.top();
        node_stack.pop();

        if (!collabsible(current_node)) {
            for (unsigned c = 0; c != CHILDREN; ++c) {
                if (current_node->child_node[c]) {
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
        else {
			current_node->error = 0.0;
			current_node->priority = 0.0;
            for (auto& c : current_node->child_node) {
				current_node->error = std::max(current_node->error, c->error);
				current_node->priority = std::max(current_node->priority, c->priority);
            }
        }
    }    
}

std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::get_leaf_nodes(QuadtreeRenderer::q_tree_ptr t) const
{
    std::vector<q_node_ptr> leaf_nodes;
    std::stack<q_node_ptr> node_stack;
    node_stack.push(t->root_node);

    q_node_ptr current_node;

    while (!node_stack.empty()){
        current_node = node_stack.top();
        node_stack.pop();

        if (!current_node->leaf){
            for (unsigned c = 0; c != CHILDREN; ++c){
                if (current_node->child_node[c]){
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
        else{
            leaf_nodes.push_back(current_node);
        }
    }
    return leaf_nodes;
}

std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::get_leaf_nodes_with_depth_outside(QuadtreeRenderer::q_tree_ptr t, const unsigned depth) const
{
    std::vector<q_node_ptr> leaf_nodes;
    std::stack<q_node_ptr> node_stack;
    node_stack.push(t->root_node);

    q_node_ptr current_node;

    while (!node_stack.empty()){
        current_node = node_stack.top();
        node_stack.pop();

        if (!current_node->leaf){
            for (unsigned c = 0; c != CHILDREN; ++c){
                if (current_node->child_node[c]){
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
        else{
            if (current_node->depth == depth && check_frustrum(current_node))
                leaf_nodes.push_back(current_node);
        }
    }
    return leaf_nodes;
}


bool
QuadtreeRenderer::is_node_inside_tree(q_node_ptr node, q_tree_ptr tree){

    size_t max_nodes_finest_level = q_layout.total_node_count_level(tree->max_depth);
    auto nodes_on_lvl = q_layout.total_node_count_level(node->depth);
    auto one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
    auto node_pos = q_layout.node_position(node->node_id);
    auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);

    size_t index = (node_pos.x * one_node_to_finest) + (resolution - 1 - ((node_pos.y) * one_node_to_finest)) * resolution;

    assert(node == node->tree->qtree_index_data[index]);

    if (tree->qtree_index_data[index]->depth >= node->depth + 1){
        return true;
    }

    return false;
}

bool
QuadtreeRenderer::is_child_node_inside_tree(q_node_ptr n, q_tree_ptr tree){


    for (unsigned c = 0; c != CHILDREN; ++c){

        ///////setting node index image
        size_t max_nodes_finest_level = q_layout.total_node_count_level(n->tree->max_depth);
        auto nodes_on_lvl = q_layout.total_node_count_level(n->child_node[c]->depth);
        auto one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
        auto node_pos = q_layout.node_position(n->child_node[c]->node_id);
        auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);

        size_t index = (node_pos.x * one_node_to_finest) + (resolution - 1 - ((node_pos.y) * one_node_to_finest)) * resolution;
        auto ideal_node = tree->qtree_index_data[index];
        
        assert(n->child_node[c] == n->tree->qtree_index_data[index]);

        if (ideal_node->depth >= n->child_node[c]->depth){
            return true;
        }
    }    
    return false;
}


void
QuadtreeRenderer::optimize_current_tree(QuadtreeRenderer::q_tree_ptr current){
	
    auto split_able_nodes = get_splitable_nodes(current);
    auto colap_able_nodes = get_collabsible_nodes(current);

    std::priority_queue<q_node_ptr, std::vector<q_node_ptr>, lesser_prio_ptr> split_able_nodes_pq(split_able_nodes.begin(), split_able_nodes.end());
    std::priority_queue<q_node_ptr, std::vector<q_node_ptr>, greater_prio_ptr> colap_able_nodes_pq(colap_able_nodes.begin(), colap_able_nodes.end());

    int split_counter = 0;

    while (!split_able_nodes_pq.empty()
        && split_counter < current->frame_budget) {

        auto curren_node = split_able_nodes_pq.top();
        split_able_nodes_pq.pop();

        //curren_node->split_mark = true;

        if (current->budget_filled < current->budget) {
            auto dependend_nodes = check_neighbors_for_split(curren_node);
            if (dependend_nodes.empty()) {
                split_node(curren_node);
                ++split_counter;
            }
        }
        else {
            bool collapsed = false;
            while (!colap_able_nodes_pq.empty()
                && !collapsed) {
                
                auto curren_col_node = colap_able_nodes_pq.top();
                colap_able_nodes_pq.pop();
                
                if((curren_col_node->priority + curren_col_node->priority * 0.001) < curren_node->priority)
                if (collabsible(curren_col_node)) {
                    auto dependend_nodes = check_neighbors_for_collapse(curren_col_node);
                    if (dependend_nodes.empty()) {
                        collapse_node(curren_col_node);
                        collapsed = true;
                    }
                }
            }

            if (collapsed) {
                auto dependend_nodes = check_neighbors_for_split(curren_node);
                if (dependend_nodes.empty()) {
                    split_node(curren_node);
                    ++split_counter;
                }
            }
        }
    }
}


void
QuadtreeRenderer::update_tree(){
    
    clear_tree_marks(m_tree_current);

    update_priorities(m_tree_current);

    optimize_current_tree(m_tree_current);

    update_importance_map(m_tree_current);
	
    m_treeInfo.used_budget = m_tree_current->budget_filled;
    
    for (auto& n : cleanup_container){
        delete n;
    }

    cleanup_container.clear();

}

void
QuadtreeRenderer::reset(){
    m_dirty = true;
}

void
QuadtreeRenderer::update_vbo(){

    {

        std::vector<q_node_ptr> leafs = get_leaf_nodes(m_tree_current);

        m_cubeVertices.clear();
        //std::cout << std::endl;
        unsigned counter = 0u;

        m_treeInfo.min_prio = 99999.0;
        m_treeInfo.max_prio = -99999.0;
        m_treeInfo.min_importance = 99999.0;
        m_treeInfo.max_importance = -99999.0;
		m_treeInfo.min_error = 99999.0;
		m_treeInfo.max_error = -99999.0;

        for (auto& l = leafs.begin(); l != leafs.end(); ++l){
            ++counter;

            auto pos = q_layout.node_position((*l)->node_id);
            auto node_level = q_layout.level_index((*l)->node_id);
            auto total_node_index = q_layout.total_node_count(node_level);

            auto max_pos = q_layout.node_position(total_node_index - 1);

            //m_treeInfo.ref_pos_trans.x = m_camera_point_trans.x * max_pos.x;
            //m_treeInfo.ref_pos_trans.y = m_camera_point_trans.y * max_pos.y;

            size_t max_nodes_finest_level = q_layout.total_node_count_level((*l)->tree->max_depth);
            auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);
            //auto camera_pos = (float)resolution * m_camera_point_trans;
            //m_treeInfo.ref_pos_trans = camera_pos;

            //std::cout << "NodeId: " << (*l)->node_id << " px: " << (float)pos.x / (max_pos.x + 1.0) << " py: " << (float)pos.y / (max_pos.y + 1.0) << " level: " << q_layout.level_index((*l)->node_id) << std::endl;

            auto v_pos = glm::vec2((float)pos.x / (max_pos.x + 1.0), (float)pos.y / (max_pos.y + 1.0));
            auto v_length = 1.0 / (max_pos.x + 1);

            QuadtreeRenderer::Vertex v_1b;
            QuadtreeRenderer::Vertex v_2b;
            QuadtreeRenderer::Vertex v_3b;
            QuadtreeRenderer::Vertex v_4b;

            float error = (*l)->error;
            //std::cout << error << std::endl;

            glm::vec4 trans = m_model * glm::vec4(v_pos, 0.0f, 1.0f);
            glm::vec2 trans2 = glm::vec2(trans.x, trans.y);

            m_treeInfo.min_prio = std::min((*l)->priority, m_treeInfo.min_prio);
            m_treeInfo.max_prio = std::max((*l)->priority, m_treeInfo.max_prio);
            m_treeInfo.min_importance = std::min((*l)->importance, m_treeInfo.min_importance);
            m_treeInfo.max_importance = std::max((*l)->importance, m_treeInfo.max_importance);
			m_treeInfo.min_error = std::min((*l)->error, m_treeInfo.min_error);
			m_treeInfo.max_error = std::max((*l)->error, m_treeInfo.max_error);


            glm::vec3 color = helper::WavelengthToRGB(helper::GetWaveLengthFromDataPoint((*l)->importance, m_treeInfo.min_prio, m_treeInfo.max_prio));// glm::vec3(0.0f, 1.0f, 0.0f);
            auto t_g = color.g;
            color.g = color.b;
            color.b = t_g;

			color = glm::vec3(1.0, 1.0, 1.0);

   //         if ((*l)->parent->dependend_mark)                
   //             color = glm::vec3(1.0, 0.0, 0.0);
			//else
			//	color = glm::vec3(0.0, 1.0, 0.0);
   //         
   //         if ((*l)->split_mark)
   //             color.b = 1.0;

			if ((*l)->checked_mark) {
				color.r = 0.0;
				color.b = 0.0;
			}
            

            //if ((*l)->importance < 0.0)
            //    color.g = 1.0;
            //else
            //    color.g = 0.0;

            v_1b.position = glm::vec3(v_pos.x, v_pos.y, 0.0f);
            v_1b.color = color;
            trans = m_model * glm::vec4(v_pos.x + v_length, v_pos.y, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            v_2b.position = glm::vec3(v_pos.x + v_length, v_pos.y, 0.0f);
            v_2b.color = color;
            trans = m_model * glm::vec4(v_pos.x + v_length, v_pos.y + v_length, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            v_3b.position = glm::vec3(v_pos.x + v_length, v_pos.y + v_length, 0.0f);
            v_3b.color = color;
            trans = m_model * glm::vec4(v_pos.x, v_pos.y + v_length, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            v_4b.position = glm::vec3(v_pos.x, v_pos.y + v_length, 0.0f);
            v_4b.color = color;
            m_cubeVertices.push_back(v_1b);
            m_cubeVertices.push_back(v_2b);
            m_cubeVertices.push_back(v_2b);
            m_cubeVertices.push_back(v_3b);
            m_cubeVertices.push_back(v_3b);
            m_cubeVertices.push_back(v_4b);
            m_cubeVertices.push_back(v_4b);
            m_cubeVertices.push_back(v_1b);
        }

    }

    {

        std::vector<q_node_ptr> leafs = get_leaf_nodes(m_tree_current);

        m_cubeVertices_i.clear();
        //std::cout << std::endl;
        unsigned counter = 0u;
        
        for (auto& l = leafs.begin(); l != leafs.end(); ++l){
            ++counter;

            auto pos = q_layout.node_position((*l)->node_id);
            auto node_level = q_layout.level_index((*l)->node_id);
            auto total_node_index = q_layout.total_node_count(node_level);

            auto max_pos = q_layout.node_position(total_node_index - 1);

            auto v_pos = glm::vec2((float)pos.x / (max_pos.x + 1.0), (float)pos.y / (max_pos.y + 1.0));
            auto v_length = 1.0 / (max_pos.x + 1);

            QuadtreeRenderer::Vertex v_1b;
            QuadtreeRenderer::Vertex v_2b;
            QuadtreeRenderer::Vertex v_3b;
            QuadtreeRenderer::Vertex v_4b;

            float error = (*l)->error;
            float depth = (*l)->depth;
            //std::cout << error << std::endl;

            glm::vec4 trans = m_model * glm::vec4(v_pos, 0.0f, 1.0f);
            glm::vec2 trans2 = glm::vec2(trans.x, trans.y);

            //m_treeInfo.min_prio = std::min((*l)->priority, m_treeInfo.min_prio);
            //m_treeInfo.max_prio = std::max((*l)->priority, m_treeInfo.max_prio);

            glm::vec3 color = helper::WavelengthToRGB(helper::GetWaveLengthFromDataPoint((*l)->priority, m_treeInfo.min_prio, m_treeInfo.max_prio));// glm::vec3(0.0f, 1.0f, 0.0f);
            //glm::vec3 color = helper::WavelengthToRGB(helper::GetWaveLengthFromDataPoint((float)(*l)->depth, 0.0, 6.0));// glm::vec3(0.0f, 1.0f, 0.0f);
            auto t_g = color.g;
            color.g = color.b;
            color.b = t_g;
            v_1b.position = glm::vec3(v_pos.x, v_pos.y, 0.0f);
            v_1b.color = color;
            trans = m_model * glm::vec4(v_pos.x + v_length, v_pos.y, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            v_2b.position = glm::vec3(v_pos.x + v_length, v_pos.y, 0.0f);
            v_2b.color = color;
            trans = m_model * glm::vec4(v_pos.x + v_length, v_pos.y + v_length, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            v_3b.position = glm::vec3(v_pos.x + v_length, v_pos.y + v_length, 0.0f);
            v_3b.color = color;
            trans = m_model * glm::vec4(v_pos.x, v_pos.y + v_length, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            v_4b.position = glm::vec3(v_pos.x, v_pos.y + v_length, 0.0f);
            v_4b.color = color;
            m_cubeVertices_i.push_back(v_1b);
            m_cubeVertices_i.push_back(v_2b);
            m_cubeVertices_i.push_back(v_2b);
            m_cubeVertices_i.push_back(v_3b);
            m_cubeVertices_i.push_back(v_3b);
            m_cubeVertices_i.push_back(v_4b);
            m_cubeVertices_i.push_back(v_4b);
            m_cubeVertices_i.push_back(v_1b);
        }
    }
    std::vector<QuadtreeRenderer::Vertex> pVertices;

    {
        QuadtreeRenderer::Vertex v_1b;
        QuadtreeRenderer::Vertex v_2b;
        QuadtreeRenderer::Vertex v_3b;
        QuadtreeRenderer::Vertex v_4b;
        QuadtreeRenderer::Vertex v_5b;
        QuadtreeRenderer::Vertex v_6b;

		glm::vec3 color_test_point = glm::vec3(1.0f, 0.0f, 0.0f);
		
		unsigned frust_nbr = 0;

		for (auto& f : m_frustrum_2d_vec) {

			v_1b.position = glm::vec3(f.m_camera_point_trans.x, f.m_camera_point_trans.y, 0.0f);
			v_1b.color = glm::vec3(1.0f, 0.0f, 0.0f);
			pVertices.push_back(v_1b);


			glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);
			if (m_restriction_direction)
				color = glm::vec3(0.0f, 0.0f, 1.0f);
			if (!m_restriction)
				color = glm::vec3(0.2f, 0.2f, 0.2f);

			v_2b.position = glm::vec3(m_restriction_line_trans[0].x, m_restriction_line_trans[0].y, 0.0f);
			v_2b.color = color;

			v_3b.position = glm::vec3(m_restriction_line_trans[1].x, m_restriction_line_trans[1].y, 0.0f);
			v_3b.color = color;

			pVertices.push_back(v_2b);
			pVertices.push_back(v_3b);

			color = glm::vec3(1.0f, 1.0f, 0.0f);

			v_4b.position = glm::vec3(f.m_frustrum_points_trans[0].x, f.m_frustrum_points_trans[0].y, 0.0f);
			v_4b.color = color;

			v_5b.position = glm::vec3(f.m_frustrum_points_trans[1].x, f.m_frustrum_points_trans[1].y, 0.0f);
			v_5b.color = color;

			pVertices.push_back(v_4b);
			pVertices.push_back(v_5b);

			if (check_frustrum(frust_nbr, m_test_point)) {
				color_test_point = glm::vec3(0.0f, 1.0f, 0.0f);
			}			
		}

		v_6b.position = glm::vec3(m_test_point_trans.x, m_test_point_trans.y, 0.0f);;
		v_6b.color = color_test_point;
        pVertices.push_back(v_6b);

    }

    std::vector<QuadtreeRenderer::Vertex> rVertices;
    {
        QuadtreeRenderer::Vertex v_1b;
        QuadtreeRenderer::Vertex v_2b;

        glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);

        if (m_restriction_direction)
            color = glm::vec3(0.0f, 0.0f, 1.0f);

        if (!m_restriction)
            color = glm::vec3(0.2f, 0.2f, 0.2f);

        v_1b.position = glm::vec3(m_restriction_line[0].x, m_restriction_line[0].y, 0.0f);
        v_1b.color = color;

        v_2b.position = glm::vec3(m_restriction_line[1].x, m_restriction_line[1].y, 0.0f);
        v_2b.color = color;

        rVertices.push_back(v_1b);
        rVertices.push_back(v_2b);
		QuadtreeRenderer::Vertex v_3b;
		QuadtreeRenderer::Vertex v_4b;
		QuadtreeRenderer::Vertex v_5b;
		QuadtreeRenderer::Vertex v_6b;

		color = glm::vec3(1.0f, 1.0f, 0.0f);

		unsigned frust_nbr = 0;

		for (auto& f : m_frustrum_2d_vec) {

			glm::vec2 ep = f.m_frustrum_points[0];

			ep += (f.m_frustrum_points[0] - f.m_camera_point) * 100.0f;

			v_3b.position = glm::vec3(f.m_camera_point.x, f.m_camera_point.y, 0.0f);
			v_3b.color = color;
			v_4b.position = glm::vec3(ep.x, ep.y, 0.0f);
			v_4b.color = color;

			ep = f.m_frustrum_points[1];
			ep += (f.m_frustrum_points[1] - f.m_camera_point) * 100.0f;

			v_5b.position = glm::vec3(f.m_camera_point.x, f.m_camera_point.y, 0.0f);
			v_5b.color = color;
			v_6b.position = glm::vec3(ep.x, ep.y, 0.0f);
			v_6b.color = color;

			rVertices.push_back(v_3b);
			rVertices.push_back(v_4b);
			rVertices.push_back(v_5b);
			rVertices.push_back(v_6b);
		}

    }


    unsigned int i = 0;

    if (m_vao)
        glDeleteBuffers(1, &m_vao);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * m_cubeVertices.size()
        , m_cubeVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);

    if (m_vao_i)
        glDeleteBuffers(1, &m_vao_i);

    glGenVertexArrays(1, &m_vao_i);
    glBindVertexArray(m_vao_i);

    if (m_vbo_i)
        glDeleteBuffers(1, &m_vbo_i);

    glGenBuffers(1, &m_vbo_i);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_i);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * m_cubeVertices_i.size()
        , m_cubeVertices_i.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);

    if (m_vao_p)
        glDeleteBuffers(1, &m_vao_p);

    glGenVertexArrays(1, &m_vao_p);
    glBindVertexArray(m_vao_p);

    if (m_vbo_p)
        glDeleteBuffers(1, &m_vbo_p);
    glGenBuffers(1, &m_vbo_p);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_p);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * pVertices.size()
        , pVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);

    if (m_vao_r)
        glDeleteBuffers(1, &m_vao_r);

    glGenVertexArrays(1, &m_vao_r);
    glBindVertexArray(m_vao_r);

    if (m_vbo_r)
        glDeleteBuffers(1, &m_vbo_r);
    glGenBuffers(1, &m_vbo_r);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_r);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * rVertices.size()
        , rVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);

}



void QuadtreeRenderer::update_and_draw(std::vector<glm::vec2> screen_pos, glm::uvec2 screen_dim)
{


    float ratio = (float)screen_dim.x / screen_dim.y;
    float ratio2 = (float)screen_dim.y / screen_dim.x;

    glm::mat4 model = glm::translate(glm::vec3(0.35f, 0.15f, 0.0f))* glm::scale(glm::vec3(0.4f, 0.4f * ratio, 1.0));
    glm::mat4 view = model * glm::mat4();
    glm::mat4 projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f);

    m_model = model;
    m_model_inverse = glm::inverse(model);
	
	unsigned frust_nbr = 0;

	for (auto& f : m_frustrum_2d_vec) {
		glm::vec4 screen_pos_trans = m_model_inverse * glm::vec4(screen_pos[frust_nbr], 0.0f, 1.0f);
		
		f.m_camera_point = screen_pos[frust_nbr];
		f.m_camera_point_trans = glm::vec2(screen_pos_trans.x, screen_pos_trans.y);

		m_treeInfo.ref_pos = f.m_camera_point;
		m_treeInfo.ref_pos_trans = f.m_camera_point_trans;

		auto lin1trans = (m_model_inverse * glm::vec4(m_restriction_line[0], 0.0f, 1.0f));
		m_restriction_line_trans[0] = glm::vec2(lin1trans.x, lin1trans.y);
		auto lin2trans = (m_model_inverse * glm::vec4(m_restriction_line[1], 0.0f, 1.0f));
		m_restriction_line_trans[1] = glm::vec2(lin2trans.x, lin2trans.y);

		auto frus1trans = (m_model_inverse * glm::vec4(f.m_frustrum_points[0], 0.0f, 1.0f));
		f.m_frustrum_points_trans[0] = glm::vec2(frus1trans.x, frus1trans.y);
		auto frus2trans = (m_model_inverse * glm::vec4(f.m_frustrum_points[1], 0.0f, 1.0f));
		f.m_frustrum_points_trans[1] = glm::vec2(frus2trans.x, frus2trans.y);

		++frust_nbr;
	}

    auto testtrans = (m_model_inverse * glm::vec4(m_test_point, 0.0f, 1.0f));
    m_test_point_trans = glm::vec2(testtrans.x, testtrans.y);

    update_tree();
    update_vbo();

#if 0
    glActiveTexture(GL_TEXTURE0);
    updateTexture2D(m_texture_id_current, m_tree_resolution, m_tree_resolution, (char*)&m_tree_current->qtree_depth_data[0], GL_RED, GL_UNSIGNED_BYTE);
#elif 0
    glActiveTexture(GL_TEXTURE0);
    updateTexture2D(m_texture_id_current, m_tree_resolution, m_tree_resolution, (char*)&m_tree_current->qtree_id_data[0], GL_RGB, GL_UNSIGNED_BYTE);
#elif 1
    glActiveTexture(GL_TEXTURE0);
    updateTexture2D(m_texture_id_current, m_tree_resolution, m_tree_resolution, (char*)&m_tree_current->qtree_importance_data[0], GL_RED, GL_FLOAT);
#else
    auto data_current = m_tree_current->get_id_data_rgb();

    glActiveTexture(GL_TEXTURE0);
    updateTexture2D(m_texture_id_current, m_tree_resolution, m_tree_resolution, (char*)&data_current[0], GL_RGB, GL_UNSIGNED_BYTE);
#endif

#if 1
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture_id_current);
    glUseProgram(m_program_texture_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_texture_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_texture_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(view));
    glUniform1i(glGetUniformLocation(m_program_texture_id, "Texture"), 0);

    glBindVertexArray(m_vao_quad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    glBindTexture(GL_TEXTURE_2D, 0);
#endif
    glm::mat4 model_s = glm::translate(glm::vec3(0.05f, 0.15f, 0.0f))* glm::scale(glm::vec3(0.2f, 0.2f * ratio, 1.0));

    glLineWidth(2.f);

    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(view));

    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, m_cubeVertices.size());
    glBindVertexArray(0);

    glUseProgram(0);


    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(model_s));

    glBindVertexArray(m_vao_i);
    glDrawArrays(GL_LINES, 0, m_cubeVertices_i.size());
    glBindVertexArray(0);

    glUseProgram(0);

    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(glm::mat4()));

    glBindVertexArray(m_vao_r);
    glDrawArrays(GL_LINES, 0, 10);
    glBindVertexArray(0);

    glUseProgram(0);

    glPointSize(20.0);

    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(view));

    glBindVertexArray(m_vao_p);
    glDrawArrays(GL_POINTS, 0, 10);
    glBindVertexArray(0);

    glUseProgram(0);

}
