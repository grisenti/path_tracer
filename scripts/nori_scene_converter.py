import sys
import xml.etree.ElementTree as ET
import numpy as np

def warn(msg):
    print(msg, file=sys.stderr)

def normalize_list_of_values(l, fn):
    values = []
    value = ''
    for c in l:
        if (c == ',' or c == ' ') and len(value) != 0:
            values.append(fn(value))
            value = ''
        else:
            value += c
    if len(value) != 0:
        values.append(fn(value))
    return values

def value_list_to_string(l):
    values = normalize_list_of_values(l, lambda e: str(e))
    return f'{values}'

def value_list_to_number_array(l):
    values = normalize_list_of_values(l, lambda e: float(e))
    return values

def parse_simple_node(node):
    name = node.attrib['name']
    match node.tag:
        case 'float' | 'integer' | 'string' | 'boolean':
            value = node.attrib['value']
            return {'name': name, 'value': value}
        case 'color':
            value = value_list_to_number_array(node.attrib['value'])
            return {'name': name, 'value': value}
        case _:
            warn(f"invalid simple node '{node.tag}'")
            return {'name': 'invalid', 'value': 'invalid'}

def parse_transform(node, view_transform=False):
    transform = np.eye(4, 4)
    contains_lookat = False
    for child in node:
        match child.tag:
            case 'matrix':
                l = child.attrib['value']
                m = np.array(value_list_to_number_array(l)).reshape((4, 4))
                transform = m @ transform
            case 'scale':
                l = child.attrib['value']
                s = value_list_to_number_array(l)
                s.append(1)
                transform = np.diag(s) @ transform
            case 'rotate':
                axis = np.array(value_list_to_number_array(child.attrib['axis']))
                axis /= np.linalg.norm(axis)
                angle = float(child.attrib['angle'])
                c = np.cos(angle * np.pi / 180)
                s = np.sin(angle * np.pi / 180)
                t = 1 - c
                x, y, z = axis
                m = np.array([
                    [c + t * x * x, t * x * y - s * z, t * x * z + s * y, 0],
                    [t * x * y + s * z, c + t * y * y, t * y * z - s * x, 0],
                    [t * x * z - s * y, t * y * z + s * x, c + t * z * z, 0],
                    [0, 0, 0, 1]
                ])
                transform = m @ transform
            case 'lookat':
                contains_lookat = True
                target = np.array(value_list_to_number_array(child.attrib['target']))
                origin = np.array(value_list_to_number_array(child.attrib['origin']))
                up = np.array(value_list_to_number_array(child.attrib['up']))
                up /= np.linalg.norm(up)
                z_c = origin - target
                z_c /= np.linalg.norm(z_c)
                x_c = np.cross(up, z_c)
                x_c /= np.linalg.norm(x_c)
                y_c = np.cross(z_c, x_c)

                m = np.eye(4, 4)
                m[:3, :3] = np.vstack([x_c, y_c, z_c])

                t = np.eye(4, 4)
                t[:3, 3] = -origin
                transform = m @ t @ transform
            case other:
                warn(f"invalid transform node '{other}'")
    if view_transform and not contains_lookat:
        # nori used world-from-view instead of view-from-world for matrix transforms
        # it also used different handedness.
        flip_camera_x_and_z = np.diag([-1, 1, -1, 1])
        transform = np.linalg.inv(transform @ flip_camera_x_and_z)
    return transform[:3, :4].flatten().tolist()

def horizontal_fov_to_vertical(fov, width, height):
    aspect_ratio = width / height
    return np.degrees(2 * np.arctan(np.tan(np.radians(fov) / 2) / aspect_ratio))

def parse_camera(node):
    assert node.attrib['type'] == 'perspective', f'camera type not supported'
    film = ''
    camera = '[camera]\ntype = "perspective"\n'
    fov = None
    width = None
    height = None
    view_transform = None
    for child in node:
        match child.tag:
            case 'float':
                match child.attrib['name']:
                    case 'fov':
                        fov = float(child.attrib['value'])
                    case 'nearClip' | 'farClip':
                        pass
                    case other:
                        warn(f"invalid camera float node '{other}'")
            case 'integer':
                match child.attrib['name']:
                    case 'width':
                        width = int(child.attrib['value'])
                    case 'height':
                        height = int(child.attrib['value'])
                    case other:
                        warn(f"invalid camera integer node '{other}'")
            case 'rfilter':
                pass
            case 'transform':
                view_transform = parse_transform(child, view_transform=True)
            case other:
                warn(f"invalid camera node '{other}'")

    if width is None or height is None:
        raise Exception('camera width and height are required')
    if fov is None:
        raise Exception('camera fov is required')
    if view_transform is None:
        raise Exception('camera transform is required')

    film += f'width = {width}\nheight = {height}\n'
    fovy = horizontal_fov_to_vertical(fov, width, height)
    camera += f'fovy = {fovy}\n'
    camera += '[camera.view_transform]\n'
    camera += 'type = "matrix"\n'
    camera += f'matrix = {view_transform}\n'
    return film, camera

def parse_spp_from_sampler(node):
    for child in node:
        if child.tag == 'integer' and child.attrib['name'] == 'sampleCount':
            return f'spp = {child.attrib["value"]}\n'

def parse_integrator(node):
    type_ = node.attrib['type']
    result = '[integrator]\n'
    match type_:
        case 'normals':
            result += 'type = "normals"\n'
        case 'random_walk':
            result += 'type = "random_walk"\n'
        case 'path_tracer_recursive' | 'simple_path':
            result += 'type = "simple_path"\n'
        case 'path':
            result += 'type = "path"\n'
        case _:
            warn(f"integrator '{type_}' is not supported")
    for child in node:
        warn(f'unsupported "{child.tag}" attribute for integrator ("{child.attrib.get("name", child.attrib.get("type", "unknown"))}")')
    return result

def parse_diffuse_brdf(node):
    albedo = None
    for child in node:
        if child.tag == 'color' and child.attrib['name'] == 'albedo':
            albedo = value_list_to_number_array(child.attrib['value'])
        else:
            warn(f'unsupported "{child.tag}" attribute for diffuse BRDF ("{child.attrib["name"]}")')
    if albedo is None:
        albedo = '[1, 1, 1]'
    return f'albedo = {albedo}\n'

def parse_dielectric_bsdf(node):
    int_ior = None
    ext_ior = None
    for child in node:
        if child.tag == 'float':
            if child.attrib['name'] == 'intIOR':
                int_ior = float(child.attrib['value'])
            elif child.attrib['name'] == 'extIOR':
                ext_ior = float(child.attrib['value'])
            else:
                warn(f'unsupported "{child.tag}" attribute for dielectric BSDF ("{child.attrib["name"]}")')
        else:
            warn(f'unsupported "{child.tag}" attribute for dielectric BSDF ("{child.attrib["name"]}")')
    if int_ior is None:
        int_ior = 1.5046
    if ext_ior is None:
        ext_ior = 1.000277
    return f'int_ior = {int_ior}\next_ior = {ext_ior}\n'


def parse_bsdf(node):
    type_ = node.attrib['type']
    result = ''
    match type_:
        case 'diffuse':
            result += 'type = "diffuse"\n'
            result += parse_diffuse_brdf(node)
        case 'dielectric':
            result += 'type = "dielectric"\n'
            result += parse_dielectric_bsdf(node)
        case 'mirror':
            result += 'type = "mirror"\n'
        case other:
            warn(f"bsdf '{other}' is not supported")
            return ''
    return result

def parse_emitter(node):
    type_ = node.attrib['type']
    result = ''
    match type_:
        case 'area' | 'parallelogram_emitter':
            result += 'type = "area"\n'
        case other:
            warn(f"emitter '{other}' is not supported")
            return ''
    for child in node:
        simple_node = parse_simple_node(child)
        result += f'{simple_node['name']} = {simple_node['value']}\n'
    return result

def parse_obj_mesh(node):
    for child in node:
        if child.tag == 'string' and child.attrib['name'] == 'filename':
            return f'filename = "{child.attrib["value"]}"\n'
    raise Exception('filename not found in obj mesh')

def parse_parallelogram(node):
    origin = None
    u = None
    v = None
    for child in node:
        match child.tag:
            case 'vector':
                match child.attrib['name']:
                    case 'u':
                        u = np.array(value_list_to_number_array(child.attrib['value']))
                    case 'v':
                        v = np.array(value_list_to_number_array(child.attrib['value']))
            case 'point':
                if child.attrib['name'] == 'origin':
                    origin = np.array(value_list_to_number_array(child.attrib['value']))
    if origin is None or u is None or v is None:
        raise Exception('invalid parallelogram')
    p0 = origin
    p1 = p0 + u
    p3 = p0 + v
    p2 = p0 + u + v
    positions = np.array([p0, p1, p2, p3]).flatten().tolist()
    indices = [0, 1, 3, 1, 2, 3]
    return f'positions = {positions}\nindices = {indices}\n'

def parse_mesh(node):
    type_ = node.attrib['type']
    result = '[[primitive]]\ntype = "object"\n'
    shape = '[primitive.shape]\n'
    if type_ == 'obj':
        shape += 'type = "obj_mesh"\n'
        shape += parse_obj_mesh(node)
    elif type_ == 'parallelogram':
        shape += 'type = "triangle_mesh"\n'
        shape += parse_parallelogram(node)
    else:
        warn(f"mesh type '{type_}' is not supported")
        return ''

    bsdf_contents = ''
    emitter_contents = ''
    for child in node:
        match child.tag:
            case 'transform':
                transform = parse_transform(child)
                shape += f'to_world = {transform}\n'
            case 'bsdf':
                bsdf_contents = parse_bsdf(child)
            case 'emitter':
                emitter_contents = parse_emitter(child)
    result += shape
    if len(bsdf_contents) != 0:
        result += '[primitive.bsdf]\n'
        result += bsdf_contents
    else:
        result += '[primitive.bsdf]\ntype = "diffuse"\nalbedo = [0.5, 0.5, 0.5]\n'
    if len(emitter_contents) != 0:
        result += '[primitive.emitter]\n'
        result += emitter_contents
    return result

assert len(sys.argv) >= 2, 'missing file argument'
filename = sys.argv[1]
document = ET.parse(filename)
scene = document.getroot()
global_settings = ''
scene_content = ''
for child in scene:
    match child.tag:
        case 'camera':
            film, camera = parse_camera(child)
            global_settings += film
            scene_content += camera
            scene_content += '\n'
        case 'sampler':
            spp = parse_spp_from_sampler(child)
            global_settings += spp
        case 'integrator':
            integrator = parse_integrator(child)
            scene_content += integrator
            scene_content += '\n'
        case 'mesh':
            scene_content += parse_mesh(child)
            scene_content += '\n'
        case other:
            warn(f"invalid scene node '{other}'")

print(global_settings)
print(scene_content)
