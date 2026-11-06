"""Prepare the independent strict build while long Debug executions run."""
import ast,qualify as h
module=ast.parse((h.AREA/'qualify.py').read_text())
targets=next(ast.literal_eval(n.value) for n in ast.walk(module) if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='targets' for t in n.targets))
h.run('strict-prebuild',['cmake','--build',h.ROOT/'build/llvm23-strict','-j2','--target',*targets],small=False)
