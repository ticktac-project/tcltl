import spot
import spot.tchecker as tc
import tempfile

# this was generated with "examples/critical-region.sh 1" in tchecker
model_txt = """
system:critical_region_1_10
event:tau
event:enter1
event:exit1
int:1:0:1:0:id
process:counter
location:counter:I{initial:}
location:counter:C{}
edge:counter:I:C:tau{provided: id==0 : do: id=1}
edge:counter:C:C:tau{provided: id<1 : do: id=id+1}
edge:counter:C:C:tau{provided: id==1 : do: id=1}
process:arbiter1
location:arbiter1:req{initial:}
location:arbiter1:ack{}
edge:arbiter1:req:ack:enter1{provided: id==1 : do: id=0}
edge:arbiter1:ack:req:exit1{do: id=1}
process:prodcell1
clock:1:x1
location:prodcell1:not_ready{initial:}
location:prodcell1:testing{invariant: x1<=10}
location:prodcell1:requesting{}
location:prodcell1:critical{invariant: x1<=20}
location:prodcell1:testing2{invariant: x1<=10}
location:prodcell1:safe{}
location:prodcell1:error{}
edge:prodcell1:not_ready:testing:tau{provided: x1<=20 : do: x1=0}
edge:prodcell1:testing:not_ready:tau{provided: x1>=10 : do: x1=0}
edge:prodcell1:testing:requesting:tau{provided: x1<=9}
edge:prodcell1:requesting:critical:enter1{do: x1=0}
edge:prodcell1:critical:error:tau{provided: x1>=20}
edge:prodcell1:critical:testing2:exit1{provided: x1<=9 : do: x1=0}
edge:prodcell1:testing2:error:tau{provided: x1>=10}
edge:prodcell1:testing2:safe:tau{provided: x1<=9}
sync:arbiter1@enter1:prodcell1@enter1
sync:arbiter1@exit1:prodcell1@exit1
"""

with tempfile.NamedTemporaryFile(dir='.', suffix='.tc') as t:
    t.write(model_txt.encode('utf-8'))
    t.flush()
    model = tc.load(t.name)

def satisfies(model, formula):
    formula = spot.formula(formula)
    n = spot.translate(spot.formula_Not(formula))
    k = model.kripke(spot.atomic_prop_collect(formula))
    return not k.intersects(n)

assert satisfies(model, 'G(arbiter1.req | arbiter1.ack)')
assert not satisfies(model, 'G(arbiter1.req -> F(arbiter1.ack))')

try:
    satisfies(model, 'G(arbiter1.req | foo)')
except RuntimeError as e:
    assert "foo" in str(e)
