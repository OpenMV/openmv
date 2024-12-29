#!/usr/bin/env python2
# This file is part of the OpenMV project.
#
# Copyright (c) 2013-2021 Ibrahim Abdelkader <iabdalkader@openmv.io>
# Copyright (c) 2013-2021 Kwabena W. Agyeman <kwagyeman@openmv.io>
#
# This work is licensed under the MIT license, see the file LICENSE for details.
#
# Haar Cascade binary converter.

import sys
import os
import struct
import argparse
from xml.dom import minidom

def print_cascade_info(path, size, stages, n_features, n_rectangles, c_format):
    C_GREEN = '\033[92m'
    C_RED = '\033[91m'
    C_BLUE = '\033[94m'
    C_RESET = '\033[0m'

    c_format = "New" if c_format else "Old"
    c_name = os.path.basename(os.path.splitext(path)[0])
    c_size = f'{size[0]}x{size[1]}'
    print(C_BLUE + f"{'Cascade:':<30} " + f"{c_name:<50}")
    print(C_BLUE + f"{'Format:':<30} " + f"{c_format:<50}")
    print(C_BLUE + f"{'Size:':<30} " + C_RED + f"{c_size:<50}")
    print(C_BLUE + f"{'Number of Stages:':<30} " + C_RED + f"{len(stages):<50}")
    print(C_BLUE + f"{'Number of Features:':<30} " + C_GREEN + f"{n_features:<50}")
    print(C_BLUE + f"{'Number of Rectangles:':<30} "+ C_GREEN + f"{n_rectangles:<50}")
    print(C_RESET)

def cascade_info_universal(path):
    xmldoc = minidom.parse(path)
    old_format = xmldoc.getElementsByTagName('stageNum').length == 0
    if old_format:
        cascade_info_old(path)
    else:
        cascade_info(path)

def cascade_info(path):
    #parse xml file
    xmldoc = minidom.parse(path)

    n_stages = int(xmldoc.getElementsByTagName('stageNum')[0].childNodes[0].nodeValue)

    # read stages
    stages_elements = xmldoc.getElementsByTagName('stages')
    stages = []
    for node in stages_elements[0].childNodes:
        if node.nodeType == 1:
            stages.append(int(node.getElementsByTagName('maxWeakCount')[0].childNodes[0].nodeValue))
    stage_threshold = xmldoc.getElementsByTagName('stageThreshold')[0:n_stages]

    # total number of features
    n_features = sum(stages)

    #read rectangles
    feature = xmldoc.getElementsByTagName('rects')[0:n_features]

    #read cascade size
    size = [int(xmldoc.getElementsByTagName('width')[0].childNodes[0].nodeValue), int(xmldoc.getElementsByTagName('height')[0].childNodes[0].nodeValue)]

    n_rectangles = 0
    for f in feature:
        rects = f.getElementsByTagName('_')
        n_rectangles = n_rectangles + len(rects)

    print_cascade_info(path, size, stages, n_features, n_rectangles, True)

def cascade_info_old(path):
    #parse xml file
    xmldoc = minidom.parse(path)

    trees = xmldoc.getElementsByTagName('trees')
    n_stages = len(trees)

    # read stages
    stages = [len(t.childNodes)//2 for t in trees][0:n_stages]
    stage_threshold = xmldoc.getElementsByTagName('stage_threshold')[0:n_stages]

    # total number of features
    n_features = sum(stages)
    # read features threshold
    threshold = xmldoc.getElementsByTagName('threshold')[0:n_features]
    #theres one of each per feature
    alpha1 = xmldoc.getElementsByTagName('left_val')[0:n_features]
    alpha2 = xmldoc.getElementsByTagName('right_val')[0:n_features]

    #read rectangles
    feature = xmldoc.getElementsByTagName('rects')[0:n_features]

    #read cascade size
    size = list(map(int, xmldoc.getElementsByTagName('size')[0].childNodes[0].nodeValue.split()))

    n_rectangles = 0
    for f in feature:
        rects = f.getElementsByTagName('_')
        n_rectangles = n_rectangles + len(rects)

    print_cascade_info(path, size, stages, n_features, n_rectangles, False)

def cascade_binary_universal(path, n_stages, name):
    xmldoc = minidom.parse(path)
    old_format = xmldoc.getElementsByTagName('stageNum').length == 0
    if old_format:
        cascade_binary_old(path, n_stages, name)
    else:
        cascade_binary(path, n_stages, name)

def cascade_binary(path, n_stages, name):
    #parse xml file
    xmldoc = minidom.parse(path)

    max_stages = int(xmldoc.getElementsByTagName('stageNum')[0].childNodes[0].nodeValue)
    if n_stages > max_stages:
        raise Exception("The max number of stages is: %d"%(max_stages))

    if n_stages == 0:
        n_stages = max_stages

    # read stages
    stages_elements = xmldoc.getElementsByTagName('stages')
    stages = []
    for node in stages_elements[0].childNodes:
        if node.nodeType == 1:
            stages.append(int(node.getElementsByTagName('maxWeakCount')[0].childNodes[0].nodeValue))
    stage_threshold = xmldoc.getElementsByTagName('stageThreshold')[0:n_stages]

    # total number of features
    n_features = int(sum(stages))

    # read features threshold
    internal_nodes = xmldoc.getElementsByTagName('internalNodes')[0:n_features]

    # theres one of each per feature
    leaf_values = xmldoc.getElementsByTagName('leafValues')[0:n_features]

    alpha1 = []
    alpha2 = []
    for val in leaf_values:
        alpha1.append(val.childNodes[0].nodeValue.split()[0])
        alpha2.append(val.childNodes[0].nodeValue.split()[1])

    # read rectangles
    feature = xmldoc.getElementsByTagName('rects')[0:n_features]

    # read cascade size
    size = [int(xmldoc.getElementsByTagName('width')[0].childNodes[0].nodeValue), int(xmldoc.getElementsByTagName('height')[0].childNodes[0].nodeValue)]

    # open output file with the specified name or xml file name
    if not name:
        name = os.path.basename(path).split('.')[0]
    fout = open(name+".cascade", "wb")

    n_rectangles = 0
    for f in feature:
        rects = f.getElementsByTagName('_')
        n_rectangles = n_rectangles + len(rects)

    # write detection window size
    fout.write(struct.pack('i', size[0]))
    fout.write(struct.pack('i', size[1]))

    # write num stages
    fout.write(struct.pack('i', len(stages)))

    # write num feat in stages
    for s in stages:
        fout.write(struct.pack('B', s)) # uint8_t

    padding = (4 - ((12 + len(stages)) % 4)) % 4
    if padding:
        fout.write(b"\x00"*padding)

    # write stages thresholds
    for t in stage_threshold:
        fout.write(struct.pack('h', int(float(t.childNodes[0].nodeValue)*256))) #int16_t

    # write features threshold 1 per feature
    for t in internal_nodes:
        fout.write(struct.pack('h', int(float(t.childNodes[0].nodeValue.split()[3])*4096))) #int16_t

    # write alpha1 1 per feature
    for a in alpha1:
        fout.write(struct.pack('h', int(float(a)*256))) #int16_t

    # write alpha2 1 per feature
    for a in alpha2:
        fout.write(struct.pack('h', int(float(a)*256))) #int16_t

    # write num_rects per feature
    for f in internal_nodes:
        idx = int(f.childNodes[0].nodeValue.split()[2])
        rects = feature[idx].getElementsByTagName('_')
        fout.write(struct.pack('B', len(rects))) # uint8_t

    # write rects weights 1 per rectangle
    for f in internal_nodes:
        idx = int(f.childNodes[0].nodeValue.split()[2])
        rects = feature[idx].getElementsByTagName('_')
        for r in rects:
            l = list(map(int, r.childNodes[0].nodeValue[:-1].split()))
            fout.write(struct.pack('b', l[4])) #int8_t NOTE: multiply by 4096

    # write rects
    for f in internal_nodes:
        idx = int(f.childNodes[0].nodeValue.split()[2])
        rects = feature[idx].getElementsByTagName('_')
        for r in rects:
            l = list(map(int, r.childNodes[0].nodeValue[:-1].split()))
            fout.write(struct.pack('BBBB', l[0], l[1], l[2], l[3])) #uint8_t


    print_cascade_info(path, size, stages, n_features, n_rectangles, True)


def cascade_binary_old(path, n_stages, name):
    #parse xml file
    xmldoc = minidom.parse(path)

    trees = xmldoc.getElementsByTagName('trees')
    max_stages = len(trees)
    if n_stages > max_stages:
        raise Exception("The max number of stages is: %d"%(max_stages))

    if n_stages == 0:
        n_stages = max_stages

    # read stages
    stages = [len(t.childNodes)//2 for t in trees][0:n_stages]
    stage_threshold = xmldoc.getElementsByTagName('stage_threshold')[0:n_stages]

    # total number of features
    n_features = sum(stages)

    # read features threshold
    threshold = xmldoc.getElementsByTagName('threshold')[0:n_features]

    # theres one of each per feature
    alpha1 = xmldoc.getElementsByTagName('left_val')[0:n_features]
    alpha2 = xmldoc.getElementsByTagName('right_val')[0:n_features]

    # read rectangles
    feature = xmldoc.getElementsByTagName('rects')[0:n_features]

    # read cascade size
    size = list(map(int, xmldoc.getElementsByTagName('size')[0].childNodes[0].nodeValue.split()))


    # open output file with the specified name or xml file name
    if not name:
        name = os.path.basename(path).split('.')[0]
    fout = open(name+".cascade", "wb")

    n_rectangles = 0
    for f in feature:
        rects = f.getElementsByTagName('_')
        n_rectangles = n_rectangles + len(rects)

    # write detection window size
    fout.write(struct.pack('i', size[0]))
    fout.write(struct.pack('i', size[1]))

    # write num stages
    fout.write(struct.pack('i', len(stages)))

    # write num feat in stages
    for s in stages:
        fout.write(struct.pack('B', s)) # uint8_t

    padding = (4 - ((12 + len(stages)) % 4)) % 4
    if padding:
        fout.write(b"\x00"*padding)

    # write stages thresholds
    for t in stage_threshold:
        fout.write(struct.pack('h', int(float(t.childNodes[0].nodeValue)*256))) #int16_t

    # write features threshold 1 per feature
    for t in threshold:
        fout.write(struct.pack('h', int(float(t.childNodes[0].nodeValue)*4096))) #int16_t

    # write alpha1 1 per feature
    for a in alpha1:
        fout.write(struct.pack('h', int(float(a.childNodes[0].nodeValue)*256))) #int16_t

    # write alpha2 1 per feature
    for a in alpha2:
        fout.write(struct.pack('h', int(float(a.childNodes[0].nodeValue)*256))) #int16_t

    # write num_rects per feature
    for f in feature:
        rects = f.getElementsByTagName('_')
        fout.write(struct.pack('B', len(rects))) # uint8_t

    # write rects weights 1 per rectangle
    for f in feature:
        rects = f.getElementsByTagName('_')
        for r in rects:
            l = list(map(int, r.childNodes[0].nodeValue[:-1].split()))
            fout.write(struct.pack('b', l[4])) #int8_t NOTE: multiply by 4096

    # write rects
    for f in feature:
        rects = f.getElementsByTagName('_')
        for r in rects:
            l = list(map(int, r.childNodes[0].nodeValue[:-1].split()))
            fout.write(struct.pack('BBBB',l[0], l[1], l[2], l[3])) #uint8_t

    print_cascade_info(path, size, stages, n_features, n_rectangles, False)


def cascade_header(path, n_stages, name):
    #parse xml file
    xmldoc = minidom.parse(path)

    trees = xmldoc.getElementsByTagName('trees')
    max_stages = len(trees)
    if n_stages > max_stages:
        raise Exception("The max number of stages is: %d"%(max_stages))

    if n_stages == 0:
        n_stages = max_stages

    # read stages
    stages = [len(t.childNodes)/2 for t in trees][0:n_stages]
    stage_threshold = xmldoc.getElementsByTagName('stage_threshold')[0:n_stages]

    # total number of features
    n_features = sum(stages)

    # read features threshold
    threshold = xmldoc.getElementsByTagName('threshold')[0:n_features]

    # theres one of each per feature
    alpha1 = xmldoc.getElementsByTagName('left_val')[0:n_features]
    alpha2 = xmldoc.getElementsByTagName('right_val')[0:n_features]

    # read rectangles
    feature = xmldoc.getElementsByTagName('rects')[0:n_features]

    # read cascade size
    size = list(map(int, xmldoc.getElementsByTagName('size')[0].childNodes[0].nodeValue.split()))

    # open output file with the specified name or xml file name
    if not name:
        name = os.path.basename(path).split('.')[0]
    fout = open(name+".h", "w")

    n_rectangles = 0
    for f in feature:
        rects = f.getElementsByTagName('_')
        n_rectangles = n_rectangles + len(rects)

    # write detection window size
    fout.write("const int %s_window_w=%d;\n" %( name, size[0]))
    fout.write("const int %s_window_h=%d;\n" %(name, size[1]))

    # write num stages
    fout.write("const int %s_n_stages=%d;\n" %(name, len(stages)))

    # write num feat in stages
    fout.write("const uint8_t %s_stages_array[]={%s};\n"
            %(name, ", ".join(str(x) for x in stages)))

    # write stages thresholds
    fout.write("const int16_t %s_stages_thresh_array[]={%s};\n"
            %(name, ", ".join(str(int(float(t.childNodes[0].nodeValue)*256)) for t in stage_threshold)))

    # write features threshold 1 per feature
    fout.write("const int16_t %s_tree_thresh_array[]={%s};\n"
            %(name, ", ".join(str(int(float(t.childNodes[0].nodeValue)*4096)) for t in threshold)))

    # write alpha1 1 per feature
    fout.write("const int16_t %s_alpha1_array[]={%s};\n"
            %(name, ", ".join(str(int(float(t.childNodes[0].nodeValue)*256)) for t in alpha1)))

    # write alpha2 1 per feature
    fout.write("const int16_t %s_alpha2_array[]={%s};\n"
            %(name, ", ".join(str(int(float(t.childNodes[0].nodeValue)*256)) for t in alpha2)))

    # write num_rects per feature
    fout.write("const int8_t %s_num_rectangles_array[]={%s};\n"
            %(name, ", ".join(str(len(f.getElementsByTagName('_'))) for f in feature)))

    # write rects weights 1 per rectangle
    rect_weights = lambda rects:", ".join(r.childNodes[0].nodeValue[:-1].split()[4] for r in rects)
    fout.write("const int8_t %s_weights_array[]={%s};\n"
            %(name, ", ".join(rect_weights(f.getElementsByTagName('_')) for f in feature)))


    # write rects
    rect = lambda rects:", ".join(", ".join(r.childNodes[0].nodeValue.split()[:-1]) for r in rects)
    fout.write("const int8_t %s_rectangles_array[]={%s};\n"
            %(name, ", ".join(rect(f.getElementsByTagName('_')) for f in feature)))

    print_cascade_info(path, size, stages, n_features, n_rectangles, False)


def main():
    # CMD args parser
    parser = argparse.ArgumentParser(description='haar cascade generator')
    parser.add_argument("-i", "--info",     action = "store_true",  help = "print cascade info and exit")
    parser.add_argument("-n", "--name",     action = "store",       help = "set cascade name", default = "")
    parser.add_argument("-s", "--stages",   action = "store",       help = "set the maximum number of stages", type = int, default=0)
    parser.add_argument("-c", "--header",   action = "store_true",  help = "generate a C header")
    parser.add_argument("file", action = "store", help = "OpenCV xml cascade file path")

    # Parse CMD args
    args = parser.parse_args()

    if args.info:
        # print cascade info and exit
        cascade_info_universal(args.file)
        return

    if args.header:
        # generate a C header from the xml cascade
        cascade_header(args.file, args.stages, args.name)
        return

    # generate a binary cascade from the xml cascade
    cascade_binary_universal(args.file, args.stages, args.name)

if __name__ == '__main__':
    main()
