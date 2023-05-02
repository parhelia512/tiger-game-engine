#include "camera.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

const double Camera::kMaxElevationAngle = 5 * pi<double>() / 12;

mat4 Camera::projection_matrix() const {
  return perspective((float)fovy_, (float)width_height_ratio_, (float)near_,
                     (float)far_);
}

void Camera::set_width_height_ratio(double width_height_ratio) {
  width_height_ratio_ = width_height_ratio;
}

Camera::Camera(vec3 position, double width_height_ratio, double alpha,
               double beta) {
  position_ = position;
  alpha_ = alpha;
  beta_ = beta;
  width_height_ratio_ = width_height_ratio;
}

double Camera::alpha() const { return alpha_; }

double Camera::beta() const { return beta_; }

void Camera::set_alpha(double alpha) { alpha_ = alpha; }

void Camera::set_beta(double beta) { beta_ = beta; }

void Camera::Rotate(double delta_alpha, double delta_beta) {
  alpha_ += delta_alpha;
  beta_ += delta_beta;
  beta_ = std::min(kMaxElevationAngle, beta_);
  beta_ = std::max(-kMaxElevationAngle, beta_);
}

vec3 Camera::position() const { return position_; }

void Camera::set_position(vec3 position) { position_ = position; }

vec3 Camera::front() const {
  return vec3(cos(alpha_) * cos(beta_), sin(beta_), sin(alpha_) * cos(beta_));
}

void Camera::set_front(vec3 new_front) {
  new_front = normalize(new_front);
  beta_ = asin(new_front.y);
  alpha_ =
      acos(new_front.x / pow(pow(new_front.x, 2) + pow(new_front.z, 2), 0.5));
  if (new_front.z < 0) alpha_ = 2 * pi<double>() - alpha_;
}

void Camera::Move(MoveDirectionType direction, float time) {
  auto left = normalize(cross(up_, front()));
  auto right = -left;
  switch (direction) {
    case MoveDirectionType::kForward:
      position_ += front() * time;
      break;
    case MoveDirectionType::kBackward:
      position_ -= front() * time;
      break;
    case MoveDirectionType::kLeftward:
      position_ += left * time;
      break;
    case MoveDirectionType::kRightward:
      position_ += right * time;
      break;
    case MoveDirectionType::kUpward:
      position_ += up_ * time;
      break;
    case MoveDirectionType::kDownward:
      position_ -= up_ * time;
  }
}

mat4 Camera::view_matrix() const {
  return lookAt(position(), position() + front(), up_);
}

vec3 Camera::center() const { return position() + front(); }

void Camera::set_center(vec3 new_center) {
  auto new_front = new_center - position_;
  set_front(new_front);
}

Frustum Camera::frustum() const {
  Frustum frustum;
  const float half_far_height = far_ * tan(fovy_ * 0.5);
  const float half_far_width = half_far_height * width_height_ratio_;
  const glm::vec3 far_vec = (float)far_ * front();
  const glm::vec3 left = glm::normalize(glm::cross(up_, front()));
  const glm::vec3 right = -left;
  const glm::vec3 up = glm::normalize(glm::cross(front(), left));

  frustum.near_plane = {front(), position() + (float)near_ * front()};
  frustum.far_plane = {-front(), position() + (float)far_ * front()};
  frustum.left_plane = {glm::cross(far_vec + left * half_far_width, up),
                        position()};
  frustum.right_plane = {glm::cross(up, far_vec + right * half_far_width),
                         position()};
  frustum.bottom_plane = {glm::cross(right, far_vec - up * half_far_height),
                          position()};
  frustum.top_plane = {glm::cross(far_vec + up * half_far_height, right),
                       position()};

  return frustum;
}