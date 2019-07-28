import subprocess
import os
import random
import sys
import json
import glob
import time
import copy
import shutil

N_CONCURRENT = 1
N_EPISODES = 10
MODEL = "model"

GAME_PATH = '../openbw/bwapi_nix/build/bin/'
MODEL_PATH = os.path.join(GAME_PATH, 'models')
RESULT_PATH = os.path.join(GAME_PATH, 'results')

os.makedirs(MODEL_PATH, exist_ok=True)
os.makedirs(RESULT_PATH, exist_ok=True)

def run(command, name, extraenv):
    env = os.environ.copy()
    env['BWAPI_CONFIG_AUTO_MENU__CHARACTER_NAME'] = name
    env.update(extraenv)
    #print(subprocess.check_output(["ls"], cwd=GAME_PATH, env=env))
    return subprocess.Popen(command, cwd=GAME_PATH, stdout=None, env=env)

def get_run_name(model, id):
    return str(model) + "_" + str(id)

def get_result_name(model, id):
    return str(model) + "_result_" + str(id)

def run_pair(model1, id1, model2, id2):
    # print("Running pair ", model1, " ", model2)
    game = str(model1) + '_' + str(model2)
    name1 = get_run_name(model1, id1)
    name2 = get_run_name(model2, id2)
    base = ['./BWAPILauncher']
    envhost = {"OPENBW_LAN_MODE": "FILE", "OPENBW_FILE_READ": "fifor", "OPENBW_FILE_WRITE": "fifow"}
    envjoin = {"OPENBW_LAN_MODE": "FILE", "OPENBW_FILE_READ": "fifow", "OPENBW_FILE_WRITE": "fifor"}
    host_process = run(base, name1, envhost)
    return { 
        'host': host_process,
        'join': run(base, name2, envjoin),
        'res_host': get_result_name(model1, id1),
        'res_join': get_result_name(model2, id2),
        }

def parse_results(w, results):
    res1file = RESULT_PATH + "/{}".format(w['res_host'])
    res2file = RESULT_PATH + "/{}".format(w['res_join'])
    if os.path.isfile(res1file) and os.path.isfile(res2file):
        # print("@@@@ RESULTS FOUND")
        results.append(w['res_host'])
        results.append(w['res_join'])
        return True
    return False

def do_round(results, generation):
    workers = [ None ] * N_CONCURRENT
    for i in range(0, N_EPISODES):
        res = run_pair(MODEL, "g{}a{}".format(generation, i), MODEL, "g{}b{}".format(generation, i))
        res['host'].wait()
        res['join'].wait()
        if not parse_results(res, results):
            print("PANTS ON MOON")
        sys.stdout.write(".")
        sys.stdout.flush()
#print(os.listdir(GAME_PATH))        
generation = 0
while True:
    results = []
    do_round(results, generation)
    results_list = os.path.join(RESULT_PATH, MODEL + "result_list_{}".format(generation))
    run_results_list = os.path.join('results', MODEL + "result_list_{}".format(generation))
    with open(results_list, "w") as f:
        for resfile in results:
            f.write("results/{}\n".format(resfile))
    model_file = "models/{}".format(MODEL)
    update_process = run(["./Overmind", "-update", model_file, run_results_list, model_file], "notused", {})
    update_process.wait()
    try:
        shutil.copytree(MODEL_PATH, "rl_gen_%d" % (generation))
        # shutil.copytree("../sc/maps/replays/ai", "rl_gen_%d/reps" % (generation))
    except Exception as e:
        print(str(e))
        print("Sad generation :( ", generation)
        pass
    # os.makedirs(MODEL_PATH, exist_ok=True)
    generation += 1
