#ifndef EMP_OPENGL_CAMERA
#define EMP_OPENGL_CAMERA

#include "math/LinAlg.h"
#include "math/region.h"

namespace emp {
  namespace opengl {
    class Camera {
      public:
      virtual math::Mat4x4f getProjection() const = 0;
      virtual math::Mat4x4f getView() const = 0;
      virtual math::Region2D<float> getRegion() const = 0;
    };

    class OrthoCamera : public Camera {
      private:
      math::Region2D<float> region;
      math::Mat4x4f projection;
      math::Mat4x4f view;

      void calculateMatrices() {
        projection = math::proj::orthoFromScreen(
          region.width(), region.height(), region.width(), region.height());
        view = math::Mat4x4f::translation(0, 0, 0);
      }

      public:
      template <class R = decltype(region)>
      OrthoCamera(R&& region)
        : region(std::forward<R>(region)),
          projection(math::proj::orthoFromScreen(
            region.width(), region.height(), region.width(), region.height())),
          view(math::Mat4x4f::translation(0, 0, 0)) {}

      template <class R = decltype(region)>
      void setRegion(R&& region) {
        this->region = std::forward<R>(region);
        calculateMatrices();
      }

      math::Region2D<float> getRegion() const override { return region; }

      math::Mat4x4f getProjection() const override { return projection; }
      math::Mat4x4f getView() const override { return view; }
    };
  }  // namespace opengl
}  // namespace emp

#endif  // EMP_OPENGL_CAMERA
