#ifndef POST_PROCESSES_H_
#define POST_PROCESSES_H_

#include "frame_buffer_object.h"
#include "shader.h"

class PostProcess {
 public:
  virtual void Resize(uint32_t width, uint32_t height) = 0;
  virtual const FrameBufferObject *fbo() const = 0;
  virtual void Draw(const FrameBufferObject *dest_fbo) = 0;
  virtual void ImGuiWindow() = 0;
};

class PostProcesses : public PostProcess {
 private:
  std::vector<std::unique_ptr<PostProcess>> post_processes_;
  std::vector<int32_t> enabled_;

 public:
  void Enable(uint32_t index, bool enabled);
  void Add(std::unique_ptr<PostProcess> post_process);
  uint32_t Size() const;
  void Resize(uint32_t width, uint32_t height) override;
  const FrameBufferObject *fbo() const override;
  void Draw(const FrameBufferObject *dest_fbo) override;
  void ImGuiWindow() override;
};

#endif