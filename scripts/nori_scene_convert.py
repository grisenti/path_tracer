import sys
import xml.etree.ElementTree as ET
import numpy as np

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
            value = value_list_to_string(node.attrib['value'])
            return {'name': name, 'value': value}
        case _:
            print(f"invalid simple node '{node.tag}'")
            return {'name': 'invalid', 'value': 'invalid'}

def parse_transform(node):
    transform = np.eye(4, 4)
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
                axis = value_list_to_number_array(child.attrib['axis'])
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
                print(f"invalid transform node '{other}'")
    return transform[:3, :4].flatten().tolist()

def parse_camera(node):
    assert node.attrib['type'] == 'perspective', f'camera type not supported'
    film = ''
    camera = '[camera]\ntype = "perspective"\n'
    for child in node:
        match child.tag:
            case 'float':
                match child.attrib['name']:
                    case 'fov':
                        camera += f'fovy = {child.attrib["value"]}\n'
                    case other:
                        print(f"invalid camera float node '{other}'")
            case 'integer':
                match child.attrib['name']:
                    case 'width':
                        film += f'width = {child.attrib["value"]}\n'
                    case 'height':
                        film += f'height = {child.attrib["value"]}\n'
                    case other:
                        print(f"invalid camera integer node '{other}'")
            case 'rfilter':
                pass
            case 'transform':
                transform = parse_transform(child)
                camera += f'view_transform = {transform}\n'
            case other:
                print(f"invalid camera node '{other}'")
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
        case _:
            print(f"integrator '{type_}' is not supported")
    return result

def parse_diffuse_brdf(node):
    albedo = None
    for child in node:
        if child.tag == 'color' and child.attrib['name'] == 'albedo':
            albedo = value_list_to_string(child.attrib['value'])
        else:
            print(f'unsupported "{child.tag}" attribute for diffuse BRDF ("{child.attrib["name"]}")')
    if albedo is None:
        albedo = '[1, 1, 1]'
    return f'albedo = {albedo}\n'


def parse_bsdf(node):
    type_ = node.attrib['type']
    result = ''
    match type_:
        case 'diffuse':
            result += 'type = "diffuse"\n'
            result += parse_diffuse_brdf(node)
        case other:
            print(f"bsdf '{other}' is not supported")
            return ''
    return result

def parse_emitter(node):
    type_ = node.attrib['type']
    result = ''
    match type_:
        case 'area':
            result += 'type = "area"\n'
        case other:
            print(f"emitter '{other}' is not supported")
            return ''
    for child in node:
        simple_node = parse_simple_node(child)
        result += simple_node['name'] + ' = ' + simple_node['value'] + '\n'
    return result

def parse_mesh(node):
    type_ = node.attrib['type']
    result = '[[primitive]]\ntype = "object"\n'
    if type_ == 'obj':
        shape_type = 'obj_mesh'
    else:
        print(f"mesh type '{type_}' is not supported")
        return ''

    shape = f'[primitive.shape]\ntype = "{shape_type}"\n'
    bsdf_contents = ''
    emitter_contents = ''
    for child in node:
        match child.tag:
            case 'string':
                match child.attrib['name']:
                    case 'filename':
                        shape += f'filename = "{child.attrib["value"]}"\n'
                    case other:
                        print(f"invalid mesh string node '{other}'")
            case 'transform':
                transform = parse_transform(child)
                shape += f'to_world = {transform}\n'
            case 'bsdf':
                bsdf_contents = parse_bsdf(child)
            case 'emitter':
                emitter_contents = parse_emitter(child)
            case other:
                print(f"invalid mesh node '{other}'")
    result += shape
    if len(bsdf_contents) != 0:
        result += '[primitive.bsdf]\n'
        result += bsdf_contents
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
            print(f"invalid scene node '{other}'")

print(global_settings)
print(scene_content)
