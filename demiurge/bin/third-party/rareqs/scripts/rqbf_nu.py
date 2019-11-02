#!/usr/bin/python
import sys,random

args=sys.argv
new_clause_freq=float(args[1])
new_q_freq=float(args[2])
nvars=int(args[3])
size=int(args[4])

vars=set(range(1, nvars+1))

qs=[]
q=[]
while vars:
  if q and (random.random() < new_q_freq): 
    qs.append(q)
    q=[]
  v=random.sample(vars, 1)[0] 
  vars.remove(v)
  q.append(v) 

if q: qs.append(q)

cls=[]
cl=set()
while size>0:
  if (len(cl)>1) and (random.random() < new_clause_freq): 
    cls.append(cl)
    cl=set()
  else:
    v=random.randint(1,nvars) 
    l=v if (random.random()<0.5) else -v
    if (l not in cl) and (-l not in cl):
      cl.add(l)
      size=size-1

if len(cl)>1: cls.append(cl)


print 'p cnf', nvars, len(cls)
for i in range(0, len(qs)):
  print  'a' if (i&1)==(len(qs)&1) else 'e',
  for v in qs[i]: print v,
  print '0'

for pc in cls:
  for l in pc: print l, 
  print '0'

