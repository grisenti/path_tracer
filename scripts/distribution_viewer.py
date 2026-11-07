from random import sample

import matplotlib.pyplot as plt
import numpy as np


def square_to_uniform_sphere(x, y):
    cos_theta = 1 - y * 2
    sin_theta = np.sqrt(np.maximum(0, 1 - cos_theta * cos_theta))
    sin_phi = np.sin(2 * np.pi * x)
    cos_phi = np.cos(2 * np.pi * x)
    return cos_phi * sin_theta, sin_phi * sin_theta, cos_theta


def square_to_uniform_hemisphere(x, y):
    cos_theta = y
    sin_theta = np.sqrt(np.maximum(0, 1 - cos_theta * cos_theta))
    sin_phi = np.sin(2 * np.pi * x)
    cos_phi = np.cos(2 * np.pi * x)
    return cos_phi * sin_theta, cos_theta, sin_phi * sin_theta


def square_to_uniform_disk(x, y):
    r = np.sqrt(x)
    theta = 2 * np.pi * y
    return r * np.cos(theta), 0, r * np.sin(theta)


def square_to_cosine_hemisphere(sx, sy):
    x, y, z = square_to_uniform_disk(sx, sy)
    return x, np.sqrt(1 - x * x - z * z), z


def square_to_beckmann(x, y, alpha):
    phi = 2 * np.pi * x
    theta = np.atan(alpha * np.sqrt(np.log(1 / (1 - y))))
    return np.sin(theta) * np.cos(phi), np.cos(theta), np.sin(theta) * np.sin(phi),


ax = plt.figure().add_subplot(projection='3d')

np.random.seed(4)
sample_count = 500
sx = np.random.sample(sample_count)
sy = np.random.sample(sample_count)
# xs, ys, zs = square_to_uniform_sphere(sx, sy)
# xs, ys, zs = square_to_uniform_hemisphere(sx, sy)
# xs, ys, zs = square_to_uniform_disk(sx, sy)
# xs, ys, zs = square_to_cosine_hemisphere(sx, sy)
xs, ys, zs = square_to_beckmann(sx, sy, 1)

ax.scatter(xs, ys, zs)

ax.set_xlim(-1, 1)
ax.set_ylim(-1, 1)
ax.set_zlim(-1, 1)
ax.set_xlabel('X')
ax.set_ylabel('Y')
ax.set_zlabel('Z')

ax.view_init(elev=32, azim=38, roll=120)
ax.set_aspect('equal')

plt.show()
