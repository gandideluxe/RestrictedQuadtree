#ifndef VOLUME_LOADER_RAW_HURRICANE_HPP
#define VOLUME_LOADER_RAW_HURRICANE_HPP

#include "data_types_fwd.hpp"

#include <array>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>


class Volume_loader_raw_hurricane
{
public:
    Volume_loader_raw_hurricane();

  volume_data_type load_volume(std::string file_path, unsigned channel, unsigned time_step);

  glm::ivec3 get_dimensions(const std::string file_path) const;
  unsigned   get_channel_count(const std::string file_path) const;
  unsigned   get_bit_per_channel(const std::string file_path) const;
  unsigned   get_time_steps(const std::string file_path) const;
  std::vector<glm::vec2>   get_channel_ranges() const;
private:

    std::vector<std::string> m_channel_names;
    std::vector<glm::vec2> m_channel_ranges;
};

#endif // define VOLUME_LOADER_RAW_HURRICANE_HPP
