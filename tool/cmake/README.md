# Resolver dependency graph

Generate the complete source-group dependency graph without configuring a port:

```sh
cmake -DOUTPUT_FILE=btstack-groups.dot -P tool/cmake/BTstackGroupGraph.cmake
dot -Tsvg btstack-groups.dot -o btstack-groups.svg
```

The first command needs only CMake. The second needs Graphviz. Alternatively,
pass `-DSVG_FILE=btstack-groups.svg` to the first command to render immediately.

Ports that register the introspection targets also provide:

```sh
cmake --build <build-dir> --target group-graph
```

This writes `btstack-groups.dot` in the port's build directory, and also writes
`btstack-groups.svg` when `dot` was found at configure time.

The graph includes every catalog group, including groups with no dependencies,
and the resolver's configuration marker groups. Arrows point from a group to its
direct requirements; transitive edges are not added. Dashed boxes are marker
groups, and dashed arrows to ellipses show `ENABLE_*` configuration requirements.

The exporter reads `btstack_group_dependencies_*` and
`btstack_group_configuration_requirements_*` directly from `BTstack.cmake`.
It shows the complete port-independent internal group graph, including legacy
aggregate groups. It does not infer dependencies from source includes, expand
aggregate source lists into new edges, or select port capability providers.
