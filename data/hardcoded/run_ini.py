#!/usr/bin/env python

import argparse
import matrix_io as mio
import smurff

def read_list(cfg, prefix):
    return [ cfg[d] for d in cfg.keys() if d.startswith(prefix) ]

def read_data(cfg, section):
    pos = cfg.get(section, "pos", fallback = None)
    if pos is not None:
        pos = map(int, pos.split(","))

    data = mio.read_matrix(cfg.get(section, "file"))
    matrix_type = cfg.get(section, "type", fallback = None)

    noise_model = cfg.get(section, "noise_model", fallback=None)
    if noise_model is not None:
        precision = cfg.getfloat(section, "precision")
        sn_init   = cfg.getfloat(section, "sn_init")
        sn_max    = cfg.getfloat(section, "sn_max")
        threshold = cfg.getfloat(section, "noise_threshold")
        noise = smurff.wrapper.NoiseConfig(noise_model, precision, sn_init, sn_max, threshold)
    else:
        noise = None

    direct = cfg.getboolean(section, "direct", fallback=None)
    tol = cfg.getfloat(section, "tol", fallback=None)

    return data, matrix_type, noise, pos, direct, tol

def read_ini(fname):
    from configparser import ConfigParser
    cfg = ConfigParser()
    cfg.read(fname)

    priors = read_list(cfg["global"], "prior_")
    seed = cfg.getint("global", "random_seed") if cfg.getboolean("global", "random_seed_set") else None
    threshold = cfg.getfloat("global", "threshold") if cfg.getboolean("global", "classify") else None 

    session = smurff.TrainSession(
        priors,
        cfg.getint("global", "num_latent"),
        cfg.getint("global", "num_threads", fallback=None),
        cfg.getint("global", "burnin"),
        cfg.getint("global", "nsamples"),
        seed, 
        threshold,
        cfg.getint("global", "verbose"),
        cfg.get   ("global", "save_name", fallback=smurff.temp_savename()),
        cfg.getint("global", "save_freq", fallback=None),
        cfg.getint("global", "checkpoint_freq", fallback=None),
    )

    data, matrix_type, noise, *_  = read_data(cfg, "train")
    session.setTrain(data, noise, matrix_type == "scarce")

    data, *_ = read_data(cfg, "test")
    session.setTest(data)

    for mode in range(len(priors)):
        section = "side_info_%d" % mode
        if section in cfg.keys():
            data, matrix_type, noise, pos, direct, tol = read_data(cfg, section) 
            session.addSideInfo(mode, data, noise, direct)

    return session

parser = argparse.ArgumentParser(description='pySMURFF - command line utility to the SMURFF Python module')

group = parser.add_argument_group("General parameters")
group.add_argument("--version", action="store_true", help="print version info (and exit)")
group.add_argument("--verbose", metavar= "NUM", type=int, default=1, help="verbose output (default = 1}")
group.add_argument("--ini", metavar="FILE", type=str,  help="read options from this .ini file")
group.add_argument("--num-threads", metavar= "NUM", type=int,  help="number of threads (0 = default by OpenMP")
group.add_argument("--seed", metavar= "NUM", type=int,  help="random number generator seed")

group = parser.add_argument_group("Used during training")
group.add_argument("--train", metavar="FILE", type=str, help="train data file")
group.add_argument("--test", metavar="FILE", type=str, help="test data")
group.add_argument("--row-features", metavar="FILE", type=str, help="sparse/dense row features")
group.add_argument("--col-features", metavar="FILE", type=str, help="sparse/dense column features")
group.add_argument("--prior", metavar="NAME", nargs=2, type=str, help="provide a prior-type for each dimension of train; prior-types:  <normal|normalone|spikeandslab|macau|macauone>")
group.add_argument("--burnin", metavar="NUM", type=int,  help="number of samples to discard")
group.add_argument("--nsamples", metavar="NUM", type=int, help="number of samples to collect")
group.add_argument("--num-latent", metavar="NUM", type=int,  help="number of latent dimensions")
group.add_argument("--threshold", metavar="NUM", type=float, help="threshold for binary classification and AUC calculation")

group = parser.add_argument_group("Storing models and predictions")
group.add_argument("--restore-from", metavar="FILE", type=str, help="restore trainSession from a saved .h5 file")
group.add_argument("--save-name", metavar="FILE", type=str, help="save model and/or predictions to this .h5 file")
group.add_argument("--save-freq", metavar="NUM", type=int, help="save every n iterations (0 == never, -1 == final model)")
group.add_argument("--checkpoint-freq", metavar="NUM", type=int, help="save state every n seconds, only one checkpointing state is kept")

args = parser.parse_args()
print(args)

if args.version:
    print("SMURFF %s" % smurff.version)
    exit

session = smurff.TrainSession()

if args.ini is not None:
    session = read_ini(args.ini)

file_options = {
    "train" : session.setTrain,
    "test" : session.setTest,
    "row-features" : lambda x: session.addSideInfo(0, x),
    "col-features" : lambda x: session.addSideInfo(1, x),
}

for opt, func in file_options.items():
    if opt in vars(args) and vars(args)[opt] is not None:
        fname = vars(args)[opt]
        data = mio.read_matrix(fname)
        func(data)

other_options = {
    "verbose" : session.setVerbose,
    "num-threads" : session.setNumThreads,
    "seed" : session.setRandomSeed,
    "prior" : session.setPriorTypes,
    "burnin" : session.setBurnin,
    "nsamples" : session.setNSamples,
    "num-latent" : session.setNumLatent,
    "threshold" : session.setThreshold,
    "restore-from" : session.setRestoreName,
    "save-name" : session.setSaveName,
    "save-freq" : session.setSaveFreq,
    "checkpoint-freq" : session.setCheckpointFreq,
}

for opt, func in other_options.items():
    if opt in vars(args) and vars(args)[opt] is not None:
        value = vars(args)[opt]
        func(value)

session.init()
