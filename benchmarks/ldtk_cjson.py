import sys
is_pkpy = not hasattr(sys, 'getrefcount')

if is_pkpy:
    import cjson as json
else:
    import json

_2489KB = 'WorldMap_GridVania_layout.ldtk'
_1093KB = 'WorldMap_Free_layout.ldtk'
_339KB = 'Typical_2D_platformer_example.ldtk'

with open(f'res/{_2489KB}', 'r') as f:
    json_content = f.read()

data: dict = json.loads(json_content)
assert isinstance(data, dict)

# dumped: str = json.dumps(data)
# loaded: dict = json.loads(dumped)
# assert len(data) == len(loaded)
# assert data == loaded