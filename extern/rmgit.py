import os
import shutil
import stat

def remove_readonly(func, path, _):
   os.chmod(path, stat.S_IWRITE)
   func(path)

# dirs to skip
blacklist = ["orca_fw-src", "orca_boot-src", "third_party_libs-src"]

def rmgit(dir):
  if any(subdir in dir for subdir in blacklist):
    print(f"Skipping {dir}")
    return
  for f in os.listdir(dir):
    f = f'{dir}/{f}'
    if os.path.isdir(f):
      if f.endswith('.git'):
        print(f"Removing {f}")
        shutil.rmtree(f, onerror=remove_readonly)
      else:
        rmgit(f)
      
rmgit('_BuildExternalDependency')