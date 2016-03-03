#!/usr/bin/python3
import argparse

def fails_criteria(pos, neg):
    assert(len(pos) == len(neg))

    val = max([abs(pos[i] -neg[i])/abs(pos[i]) for i in range(len(pos))])
    return val >= TAU


def filter(positive, negative):
    assert(len(positive) == len(negative))

    for i in range(len(positive)):
        if fails_criteria(positive[i], negative[i]):
            negative[i] = None
            positive[i] = None

    return ([p for p in positive if p!=None], [n for n in negative if n!=None])


def split_data(data):
    positive = list()
    negative = list()
    
    for line in data:
        if line.startswith("-1"):
            negative.append(line)
        else:
            positive.append(line)

    return (positive, negative)


def vectorize(data):
    vectors = list()
    for line in data:
        line = line.split()[1:] # remove indicator
        vec = [float(item.split(":")[1]) for item in line]
        vectors.append(vec)

    return vectors


def dumps(prefix, vectors):
    for vec in vectors:
        pval = [prefix]
        for i, item in enumerate(vec):
            pval.append("{}:{}".format(i+1, item))
        print(' '.join(pval))

TAU=None
def main():
    global TAU
    parser = argparse.ArgumentParser()
    parser.add_argument("infile", type=str)
    parser.add_argument("t", type=float)

    args = parser.parse_args()

    infile = args.infile
    TAU = args.t

    with open(infile, 'r') as f:
        data = f.readlines()

    data = [d for d in data if d.strip()!='']
    (pos_strings, neg_strings) = split_data(data)
    positive = vectorize(pos_strings)
    negative = vectorize(neg_strings)

    (new_positive, new_negative) = filter(positive, negative)
    dumps("-1", new_negative)
    dumps("+1", new_positive)

if __name__ == "__main__":
    main()
