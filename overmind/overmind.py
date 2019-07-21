import subprocess
import os
import random
import sys
import json
import glob
import time
import copy
import shutil

N_PARAMS = 9
N_ACTIONS = 3
N_MODELS = 6
N_CONCURRENT = 2

MODEL_PATH = "../sc/models"
RESULT_PATH = "../sc/results"

PREFIXES = ['wine0', 'wine1', 'wine2', 'wine3']

os.makedirs(MODEL_PATH, exist_ok=True)
os.makedirs(RESULT_PATH, exist_ok=True)

models = dict()

def generate_params():
    return [2*random.random() - 1 for i in range(0, N_PARAMS)]

def generate_model():
    params = [generate_params() for i in range(0, N_ACTIONS)]
    return {
        'brain': {
            'params': params
        }
     }




def get_prefix(prefix):
    return "/home/herden/projects/scai/overmind/wine" + str(prefix)

def run_in_prefix(command, prefix, res=None):
    env = os.environ.copy()
    env['WINEPREFIX'] = get_prefix(prefix)
    if res is not None:
        env['resultfile'] = res
    return subprocess.Popen(command, cwd='../sc', stdout=None, env=env)

def run_pair(prefix, model1, model2):
    print("Running pair ", model1, " ", model2)
    game = str(model1) + '_' + str(model2)
    base = ['wine', 'bwheadless.exe', '-e', 'StarCraft.exe', '-r', 'terran', '-l', 'bwapi-data/BWAPI.dll', '--localpc']
    host = base + ['-m', 'maps/ai.scm', '--host', '-g', game, '--name', str(model1)]
    join = base + ['--join', '-g', game, '--name', str(model2)]
    host_process = run_in_prefix(host, prefix, RESULT_PATH + "/{}".format(game))
    return { 
        'host': host_process,
        'host_model': str(model1), 
        'join_model': str(model2), 
        'join': run_in_prefix(join, prefix),
        'deadline': time.time() + 20 * 1,
        'delete': False,
        'game': game
        }

def did_finish(w, models):
    resfile = RESULT_PATH + "/{}".format(w['game'])
    if os.path.isfile(resfile):
        res = open(resfile, 'r').read().strip()
        print("THE RESULT: ", res)
        os.unlink(resfile)
        if res == "1":
            models[w['host_model']]['win'] += 1
            models[w['join_model']]['lose'] += 1
        else:
            models[w['host_model']]['lose'] += 1
            models[w['join_model']]['win'] += 1
        return True
    return False

def update_workers(models, workers):
    happy = 0
    for prefix, w in enumerate(workers):
        if w is None or w['delete']:
            happy += 1
            continue
        if time.time() > w['deadline'] or did_finish(w, models):
            print("TEEERMINATING")
            w['host'].terminate()
            w['join'].terminate()
            print("WAAAAAAITING")
            w['host'].wait()
            w['join'].wait()
            run_in_prefix(["wineserver", "-k"], prefix)
            print("HAAAAAAAPY")
            w['delete'] = True
    return happy
        
def do_round(models):
    workers = [ None ] * N_CONCURRENT
    for i in range(0, N_MODELS):
        for j in range(i+1, N_MODELS):
            for xxx in range(0, len(workers)):
                if not workers[xxx]:
                    workers[xxx] = run_pair(xxx, i, j)
                    break
            while True:
                happy = update_workers(models, workers)
                #happy = 0
                #for prefix, w in enumerate(workers):
                #    if not w:
                #        happy += 1
                #        continue
                #    if time.time() > w['deadline'] or did_finish(w, models):
                #        print("TEEERMINATING")
                #        w['host'].terminate()
                #        w['join'].terminate()
                #        print("WAAAAAAITING")
                #        w['host'].wait()
                #        w['join'].wait()
                #        run_in_prefix(["wineserver", "-k"], prefix)
                #        print("HAAAAAAAPY")
                #        w['delete'] = True
                workers = [w if w and not w['delete'] else None for w in workers]
                if happy != 0:
                    break
                time.sleep(1)
    while(N_CONCURRENT != update_workers(models, workers)):
        print("WAITING FOR LAST WORKERS")
        time.sleep(1)
            

if len(sys.argv) >= 2:
    if "--clean" == sys.argv[1]:
        for i in range(0, N_MODELS): 
            model = generate_model()
            path = '%s/%d' % (MODEL_PATH, i)
            open(path, 'w').write(json.dumps(model))
            models[str(i)] = { 'win': 0, 'lose': 0, 'model': model, 'path': path}

def write_models(models):
    for model in models.values(): 
        open(model['path'], 'w').write(json.dumps(model['model']))

def mutate(params):
    newparams = copy.deepcopy(params)
    for a in range(0, N_ACTIONS):
        for p in range(0, N_PARAMS):
            newparams[a][p] = params[a][p] * (1 + (2*random.random() - 1) * 0.1) 
    return newparams

def create_model(params, index):
    path = '%s/%d' % (MODEL_PATH, index)
    return dict(win=0, lose=0, model=dict(brain=dict(params=params)), path=path)

def modify_models(keep):
    models = {}
    index = 0
    for k in keep:
        for i in range(0, int(N_MODELS / len(keep))):
            models[str(index)] = create_model(mutate(k['model']['brain']['params']), index)
            index += 1
    
    models[str(N_MODELS-1)]['model'] = generate_model()
    return models
    
generation = 0
while True:
    do_round(models)
    try:
        shutil.move(MODEL_PATH, "gen_%d" % (generation))
        shutil.move("../sc/maps/replays/ai", "gen_%d/reps" % (generation))
        open('gen_%d/models.json' % (generation), 'w').write(json.dumps(models))
    except:
        print("Sad generation :( ", generation)
        pass
    os.makedirs(MODEL_PATH, exist_ok=True)
    generation += 1
    s = sorted(list(models.items()), key=lambda m: m[1]['win'])
    s.reverse()
    keep = [x[1] for x in s[:2]]
    models = modify_models(keep)
    write_models(models)
    print(models)
