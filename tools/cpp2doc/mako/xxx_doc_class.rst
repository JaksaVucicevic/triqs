<%
 import cpp2doc_tools as tools
 from cpp2doc_tools import make_table
 tools.class_list = class_list
 incl = c.doc_elements['include']
%>
..
   Generated automatically using the command :
   ${shell_command}

.. highlight:: c

.. _${c.name}:

%if incl:
.. code-block:: c

    #include<${incl}>
%endif

${c.name}
${'=' * (len(c.name)+2)}

**Synopsis**:

.. code-block:: c

     ${tools.make_synopsis_template_decl(c.tparams)} class ${c.name};

${c.processed_doc}

%if len(c.type_alias) > 0:
Member types
-----------------

${make_table(['Member type','Comment'], [(t.name, t.doc) for t in c.type_alias])}
%endif

%if len(c.all_m) > 0:
Member functions
-----------------

${make_table(['Member function','Comment'], [(":ref:`%s <%s_%s>`"%(name,c.name, name), m[0].brief_doc) for name, m in c.all_m.items()])}

.. toctree::

  :hidden:

%for m_name in c.all_m:
   ${c.name}/${m_name}
%endfor
%endif

%if len(c.all_friend_functions) > 0:
Non Member functions
-----------------------

${make_table(['Non member function','Comment'],
           [(":ref:`%s <%s_%s>`"%(name,c.name, name), m[0].brief_doc) for name, m in c.all_friend_functions.items()]) }

.. toctree::

  :hidden:
  
%for m_name in c.all_friend_functions:
   ${c.name}/${m_name}
%endfor
%endif

<% 
  code,d1,d2, s,e = tools.prepare_example(c.name, 4)
%>
%if code is not None:

Example
---------

${d1}

.. triqs_example::

    linenos:${s},${e}

${code}

${d2}    

%endif

