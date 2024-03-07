import coverage
import atexit
import random

def main(x: int, y:int):
    """Just a random function for demo purposes"""
    if x > y:
        return x
    else:
        if x == 10:
            return 13
        if y == 10:
            return 14
        if x == 5:
            return 15
        if y == 5:
            return 16
        if x == y:
            return 3
        else:
            return y

def atexit_handler():
    """Coverage saving must be registered as atexit in case of crashing"""
    cov.stop()
    cov.save()
    
atexit.register(atexit_handler)

while True:
    cov = coverage.Coverage(branch=True)
    cov.start()
    x = random.randint(0, 20)
    y = random.randint(0, 20)
    main(int(x), int(y))
    assert False

