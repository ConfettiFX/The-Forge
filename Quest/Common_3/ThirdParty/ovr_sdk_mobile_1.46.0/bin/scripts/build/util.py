import os


class chdir:
    def __init__(self, path):
        self.path = path

    def __enter__(self):
        self.old_dir = os.getcwd()
        os.chdir(self.path)

    def __exit__(self, exc_type, exc_value, traceback):
        os.chdir(self.old_dir)
