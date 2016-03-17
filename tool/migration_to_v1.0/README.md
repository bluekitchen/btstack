# BTstack v1.0 Migration

## Migrate your BTstack project from version 0.9 to 1.0.

BTstack v1.0 has undergone major API changes. To help migrate your project, the *./migration.sh* script applies a set of rules to your source files. In addition to *sed*, [Coccinelle](http://coccinelle.lip6.fr/) is used for advanced patches.

BlueKitchen GmbH is providing a [web service](http://buildbot.bluekitchen-gmbh.com/migration) to help migrate your sources if you don't want to install Coccinelle.
