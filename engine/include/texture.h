#ifndef TEXTURE_H_
#define TEXTURE_H_

#include <string>
#include <vector>

class Texture {
 private:
  uint32_t target_ = 0, id_ = 0;

  void Load2DTextureFromPath(const std::string &path, uint32_t wrap,
                             uint32_t min_filter, uint32_t mag_filter,
                             const std::vector<float> &border_color,
                             bool mipmap);

  void LoadCubeMapTextureFromPath(const std::string &path, uint32_t wrap,
                                  uint32_t min_filter, uint32_t mag_filter,
                                  const std::vector<float> &border_color,
                                  bool mipmap);

  inline void MoveTexture(Texture &&texture) {
    this->target_ = texture.target_;
    this->id_ = texture.id_;
    texture.target_ = 0;
    texture.id_ = 0;
  }

 public:
  inline Texture() = default;
  inline Texture(Texture &texture) { MoveTexture(std::move(texture)); }
  Texture &operator=(Texture &texture) {
    MoveTexture(std::move(texture));
    return *this;
  }

  // load from image
  explicit Texture(const std::string &path, uint32_t wrap, uint32_t min_filter,
                   uint32_t mag_filter, const std::vector<float> &border_color,
                   bool mipmap);

  // create 2D texture
  explicit Texture(uint32_t width, uint32_t height, uint32_t internal_format,
                   uint32_t format, uint32_t type, uint32_t wrap,
                   uint32_t min_filter, uint32_t mag_filter,
                   const std::vector<float> &border_color, bool mipmap);

  // create 2D_ARRAY/3D texture
  explicit Texture(uint32_t target, uint32_t width, uint32_t height,
                   uint32_t depth, uint32_t internal_format, uint32_t format,
                   uint32_t type, uint32_t wrap, uint32_t min_filter,
                   uint32_t mag_filter, const std::vector<float> &border_color,
                   bool mipmap);

  inline uint32_t target() const { return target_; }
  inline uint32_t id() const { return id_; }

  void Clear();

  ~Texture();
};

#endif