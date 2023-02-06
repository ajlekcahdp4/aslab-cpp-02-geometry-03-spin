/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, <alex.rom23@mail.ru> wrote this file.  As long as you
 * retain this notice you can do whatever you want with this stuff. If we meet
 * some day, and you think this stuff is worth it, you can buy me a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

#include "ezvk/debug.hpp"
#include "ezvk/device.hpp"
#include "ezvk/error.hpp"
#include "ezvk/queues.hpp"
#include "ezvk/window.hpp"
#include "glfw_include.hpp"
#include "vertex.hpp"
#include "vulkan_hpp_include.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include "broadphase/broadphase_structure.hpp"
#include "broadphase/bruteforce.hpp"
#include "broadphase/octree.hpp"
#include "broadphase/uniform_grid.hpp"

#include "narrowphase/collision_shape.hpp"
#include "primitives/plane.hpp"
#include "primitives/triangle3.hpp"
#include "vec3.hpp"

#include <chrono>
#include <cmath>
#include <concepts>
#include <iostream>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "application.hpp"

#include <boost/program_options.hpp>
#include <boost/program_options/option.hpp>
namespace po = boost::program_options;

struct indexed_geom : public throttle::geometry::collision_shape<float> {
  unsigned index;
  indexed_geom(unsigned idx, auto &&base) : collision_shape{base}, index{idx} {};
};

using throttle::geometry::collision_shape;
using throttle::geometry::is_roughly_equal;
using throttle::geometry::point3;
using throttle::geometry::segment3;
using throttle::geometry::triangle3;
using throttle::geometry::vec3;

template <typename T>
throttle::geometry::collision_shape<T> shape_from_three_points(const point3<T> &a, const point3<T> &b,
                                                               const point3<T> &c) {
  auto ab = b - a, ac = c - a;

  if (throttle::geometry::colinear(ab, ac)) { // Either a segment or a point
    if (is_roughly_equal(ab, vec3<T>::zero()) && is_roughly_equal(ac, vec3<T>::zero())) {
      return throttle::geometry::barycentric_average<T>(a, b, c);
    }
    // This is a segment. Project the the points onto the most closely alligned axis.
    auto max_index = ab.max_component().first;

    std::array<std::pair<point3<T>, T>, 3> arr = {std::make_pair(a, a[max_index]), std::make_pair(b, b[max_index]),
                                                  std::make_pair(c, c[max_index])};
    std::sort(arr.begin(), arr.end(),
              [](const auto &left, const auto &right) -> bool { return left.second < right.second; });
    return segment3<T>{arr[0].first, arr[2].first};
  }

  return triangle3<T>{a, b, c};
}

static unsigned apporoximate_optimal_depth(unsigned number) {
  constexpr unsigned max_depth = 6;
  unsigned           log_num = std::log10(float(number));
  return std::min(max_depth, log_num);
}

using wireframe_vertices_t = std::vector<triangles::wireframe_vertex_type>;

template <typename T>
void convert_to_cube_edges(wireframe_vertices_t &vertices, const glm::vec3 &min_corner, const T width) {
  std::array<triangles::wireframe_vertex_type, 24> vertices_arr{
      // 1 - 2
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1], min_corner[2]}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1], min_corner[2]}, 0u},
      // 2 - 3
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1], min_corner[2]}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1] + width, min_corner[2]}, 0u},
      // 3 - 4
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1] + width, min_corner[2]}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1] + width, min_corner[2]}, 0u},
      // 4 - 1
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1] + width, min_corner[2]}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1], min_corner[2]}, 0u},
      // 5 - 6
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1], min_corner[2] + width}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1], min_corner[2] + width}, 0u},
      // 6 - 7
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1], min_corner[2] + width}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1] + width, min_corner[2] + width}, 0u},
      // 7 - 8
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1] + width, min_corner[2] + width}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1] + width, min_corner[2] + width}, 0u},
      // 8 - 5
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1] + width, min_corner[2] + width}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1], min_corner[2] + width}, 0u},
      // 1 - 5
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1], min_corner[2]}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1], min_corner[2] + width}, 0u},
      // 2 - 6
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1], min_corner[2]}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1], min_corner[2] + width}, 0u},
      // 3 - 7
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1] + width, min_corner[2]}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0] + width, min_corner[1] + width, min_corner[2] + width}, 0u},
      // 4 - 8
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1] + width, min_corner[2]}, 0u},
      triangles::wireframe_vertex_type{{min_corner[0], min_corner[1] + width, min_corner[2] + width}, 0u}};

  vertices.insert(vertices.end(), vertices_arr.begin(), vertices_arr.end());
}

template <typename T>
void fill_wireframe_vertices(wireframe_vertices_t &vertices, throttle::geometry::bruteforce<T, indexed_geom> &) {}

template <typename T>
void fill_wireframe_vertices(wireframe_vertices_t &vertices, throttle::geometry::octree<T, indexed_geom> &octree) {
  vertices.reserve(vertices.size() * 24);
}

template <typename T>
void fill_wireframe_vertices(wireframe_vertices_t                              &vertices,
                             throttle::geometry::uniform_grid<T, indexed_geom> &uniform) {
  vertices.reserve(vertices.size() * 24);
  auto cell_size = uniform.cell_size();
  for (const auto &elem : uniform) {
    glm::vec3 cell = {elem.second[0] * cell_size, elem.second[1] * cell_size, elem.second[2] * cell_size};
    convert_to_cube_edges(vertices, cell, cell_size);
  }
}

template <typename broad>
bool application_loop(std::istream &is, throttle::geometry::broadphase_structure<broad, indexed_geom> &cont,
                      std::vector<triangles::triangle_vertex_type> &vertices, wireframe_vertices_t &wireframe_vertices,
                      unsigned n, bool hide = false) {
  using point_type = typename throttle::geometry::point3<float>;
  using triangle_type = typename throttle::geometry::triangle3<float>;
  std::vector<triangle_type> triangles;

  triangles.reserve(n);
  for (unsigned i = 0; i < n; ++i) {
    point_type a, b, c;
    if (!(is >> a[0] >> a[1] >> a[2] >> b[0] >> b[1] >> b[2] >> c[0] >> c[1] >> c[2])) {
      std::cout << "Can't read i-th = " << i << " triangle\n";
      return false;
    }
    cont.add_collision_shape({i, shape_from_three_points(a, b, c)});
    triangles.push_back({a, b, c});
  }

  auto result = cont.many_to_many();
  if (hide) return true;

  std::unordered_set<unsigned> intersecting;

  for (const auto &v : result) {
    intersecting.insert(v->index);
  }

  constexpr uint32_t intersect_index = 1u, regular_index = 0u;

  vertices.reserve(6 * n);
  std::for_each(triangles.begin(), triangles.end(), [i = 0, &intersecting, &vertices](auto triangle) mutable {
    triangles::triangle_vertex_type vertex;

    auto norm = triangle.norm();
    vertex.color_index = (intersecting.contains(i) ? intersect_index : regular_index);
    vertex.norm = {norm[0], norm[1], norm[2]};

    vertex.pos = {triangle.a[0], triangle.a[1], triangle.a[2]};
    vertices.push_back(vertex);
    vertex.pos = {triangle.b[0], triangle.b[1], triangle.b[2]};
    vertices.push_back(vertex);
    vertex.pos = {triangle.c[0], triangle.c[1], triangle.c[2]};
    vertices.push_back(vertex);

    ++i;
  });

  // Here we add triangles oriented in the opposite direction to light them differently
  auto sz = vertices.size();
  for (unsigned i = 0; i < sz; i += 3) {
    vertices.push_back(vertices[i]);
    vertices.push_back(vertices[i + 2]);
    vertices.push_back(vertices[i + 1]);
  }

  fill_wireframe_vertices(wireframe_vertices, cont.impl());

  return true;
}

int main(int argc, char *argv[]) {
  // intersection
  std::istream *isp = &std::cin;
  bool          hide = false;

  std::string             opt, input;
  po::options_description desc("Available options");
  desc.add_options()("help,h", "Print this help message")("measure,m", "Print perfomance metrics")(
      "hide", "Hide output")("broad", po::value<std::string>(&opt)->default_value("octree"),
                             "Algorithm for broad phase (bruteforce, octree, uniform-grid)")(
      "input,i", po::value<std::string>(&input), "Optional input file to use instead of stdin");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  std::ifstream ifs;
  ifs.exceptions(ifs.exceptions() | std::ifstream::badbit | std::ifstream::failbit);
  if (vm.count("input")) {
    ifs.open(input);
    isp = &ifs;
  }

  bool measure = vm.count("measure");
  hide = vm.count("hide");

  unsigned n;
  if (!(*isp >> n)) {
    std::cout << "Can't read number of triangles\n";
    return 1;
  }

  std::vector<triangles::triangle_vertex_type>  vertices;
  wireframe_vertices_t                          wireframe_vertices;

  auto start = std::chrono::high_resolution_clock::now();

  if (opt == "octree") {
    throttle::geometry::octree<float, indexed_geom> octree{apporoximate_optimal_depth(n)};
    if (!application_loop(*isp, octree, vertices, wireframe_vertices, n, hide)) return 1;
  } else if (opt == "bruteforce") {
    throttle::geometry::bruteforce<float, indexed_geom> bruteforce{n};
    if (!application_loop(*isp, bruteforce, vertices, wireframe_vertices, n, hide)) return 1;
  } else if (opt == "uniform-grid") {
    throttle::geometry::uniform_grid<float, indexed_geom> uniform{n};
    if (!application_loop(*isp, uniform, vertices, wireframe_vertices, n, hide)) return 1;
  }

  auto finish = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration<double, std::milli>(finish - start);

  if (measure) {
    std::cout << opt << " took " << elapsed.count() << "ms to run\n";
  }

  // visualizations

  ezvk::enable_glfw_exceptions();
  spdlog::cfg::load_env_levels(); // Read logging level from environment variables
  glfwInit();

  static constexpr auto app_info = vk::ApplicationInfo{.pApplicationName = "Hello, World!",
                                                       .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                                       .pEngineName = "Junk Inc.",
                                                       .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                                       .apiVersion = VK_MAKE_VERSION(1, 1, 0)};

  vk::raii::Context ctx;

  auto extensions = triangles::required_vk_extensions();

#ifdef USE_DEBUG_EXTENSION
  auto layers = triangles::required_vk_layers(true);
#else
  auto                   layers = triangles::required_vk_layers();
#endif

  ezvk::instance raw_instance = {ctx, app_info, extensions.begin(), extensions.end(), layers.begin(), layers.end()};

#ifdef USE_DEBUG_EXTENSION
  ezvk::generic_instance instance = ezvk::debugged_instance{std::move(raw_instance)};
#else
  ezvk::generic_instance instance = std::move(raw_instance);
#endif

  auto physical_device_extensions = triangles::required_physical_device_extensions();
  auto suitable_physical_devices = ezvk::physical_device_selector::enumerate_suitable_physical_devices(
      instance(), physical_device_extensions.begin(), physical_device_extensions.end());

  if (suitable_physical_devices.empty()) {
    throw ezvk::vk_error{"No suitable physical devices found"};
  }

  auto p_device = std::move(suitable_physical_devices.front());
  auto window = ezvk::unique_glfw_window{"Triangles intersection", vk::Extent2D{800, 600}, true};
  auto surface = ezvk::surface{instance(), window};
  triangles::applicaton_platform platform = {std::move(instance), std::move(window), std::move(surface),
                                             std::move(p_device)};

  auto &app = triangles::application::instance().get(&platform);

  app.load_triangles(vertices);
  app.load_wireframe(wireframe_vertices);

  while (!glfwWindowShouldClose(app.window())) {
    glfwPollEvents();
    app.loop();
  }

  app.shutdown();

  triangles::application::instance().destroy();
}