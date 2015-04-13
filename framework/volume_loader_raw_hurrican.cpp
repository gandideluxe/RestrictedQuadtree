#include "volume_loader_raw_hurrican.hpp"

#include <iostream>
#include <sstream>      // std::stringstream
#include <fstream>

Volume_loader_raw_hurricane::Volume_loader_raw_hurricane(){

    m_channel_names.clear();
    m_channel_ranges.clear();
    m_channel_names.push_back("QCLOUDf");
    m_channel_ranges.push_back({ 0.0f, 0.00332f });
    m_channel_names.push_back("QGRAUPf");
    m_channel_ranges.push_back({ 0.0f, 0.01638f });
    m_channel_names.push_back("QICEf");
    m_channel_ranges.push_back({ 0.0f, 0.00099f });
    m_channel_names.push_back("QSNOWf");
    m_channel_ranges.push_back({ 0.0f, 0.00135f });
    m_channel_names.push_back("QVAPORf");
    m_channel_ranges.push_back({ 0.0f, 0.02368f });
    m_channel_names.push_back("CLOUDf");
    m_channel_ranges.push_back({ 0.0f, 0.00332f});
    m_channel_names.push_back("PRECIPf");
    m_channel_ranges.push_back({ 0.0f, 0.01672f });
    m_channel_names.push_back("QRAINf");
    m_channel_ranges.push_back({ 0.0f, 0.00099f });
    m_channel_names.push_back("Pf");
    m_channel_ranges.push_back({ -5471.85791f, 3225.42578f });
    m_channel_names.push_back("TCf");
    m_channel_ranges.push_back({ -83.00402f, 31.51576f });
    m_channel_names.push_back("Uf");
    m_channel_ranges.push_back({ -79.47297f, 85.17703f });
    m_channel_names.push_back("Vf");
    m_channel_ranges.push_back({ -76.03391f, 82.95293f });
    m_channel_names.push_back("Wf");
    m_channel_ranges.push_back({ -9.06026f, 28.61434f });
}

std::vector<glm::vec2>   
Volume_loader_raw_hurricane::get_channel_ranges() const{
    return m_channel_ranges;
}

volume_data_type
Volume_loader_raw_hurricane::load_volume(std::string filepath, unsigned channel, unsigned time_step)
{    
    std::string fileprefix = m_channel_names[channel];
    
    filepath.append(fileprefix);

    ++time_step;

    if (time_step < 10)
        filepath.append("0");
    
    filepath.append(std::to_string(time_step)).append(".bin");

    std::cout << "LOADING FILE: " << filepath;// << std::endl;

  std::ifstream volume_file;
  volume_file.open(filepath, std::ios::in | std::ios::binary);

  volume_data_type data;

  if (volume_file.is_open()) {
    glm::ivec3 vol_dim = get_dimensions(filepath);
    //unsigned channels = get_channel_count(filepath);
    unsigned byte_per_channel = get_bit_per_channel(filepath) / 8;

    size_t data_size = vol_dim.x
                      * vol_dim.y
                      * vol_dim.z
                      * 1
                      * byte_per_channel;
            
    data.resize(data_size);

    volume_file.seekg(0, std::ios::beg);
    volume_file.read((char*)&data.front(), data_size);
    volume_file.close();

    //std::cout << "File " << filepath << " successfully loaded" << std::endl;

    return data;
  } else {
    std::cerr << "File " << filepath << " doesnt exist! Check Filepath!" << std::endl;
    assert(0);
    return data;
  }

  //never reached
  assert(0);
  return data;
}

glm::ivec3 Volume_loader_raw_hurricane::get_dimensions(const std::string filepath) const
{
  unsigned width = 500;
  unsigned height = 500;
  unsigned depth = 100;

  return glm::ivec3(width, height, depth);
}

unsigned Volume_loader_raw_hurricane::get_channel_count(const std::string filepath) const
{
    unsigned channels = m_channel_names.size();

  return channels;
}

unsigned Volume_loader_raw_hurricane::get_bit_per_channel(const std::string filepath) const
{
  unsigned byte_per_channel = 4;

  return byte_per_channel * 8;
}
