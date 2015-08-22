from mako.template import Template
fi = open("gf.mako.hpp",'r').read()

for cls in ['', 'view', 'const_view'] : 
    tpl = Template(fi.replace('GF','${GF}'))
    gf = "gf_" + cls if cls else 'gf'
    rendered = tpl.render(GF= gf, GRVC = cls if cls else 'regular' ).strip()
    with open("%s.hpp"%gf,'w') as f:
        f.write(rendered)

