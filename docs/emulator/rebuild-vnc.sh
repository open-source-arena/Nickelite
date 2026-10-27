#!/bin/sh
set -eu
python3 - <<'PY'
from pathlib import Path
p=Path('/tc/src/qtbase/src/platformsupport/fbconvenience/qfbwindow.cpp')
s=p.read_text()
before='''    Q_UNUSED(region);
    Q_UNUSED(flags);
}'''
after='''    Q_UNUSED(flags);
    repaint(region);
}'''
assert s.count(before)==1
p.write_text(s.replace(before,after))
PY
cmake --build /tc/src/target --target QVncIntegrationPlugin --parallel 4
cp /tc/src/target/plugins/platforms/libqvnc.so /out/libqvnc-repaint.so
