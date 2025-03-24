#include "mod.h"

#include "AXELF.MOD.h"
#include "cartoon_dreams_n_fantasies.mod.h"
#include "IRONMAN.MOD.h"
#include "nao_deceased_by_disease.mod.h"

const mod_title_t titles[] = {
    {
        .len = 113192,
        .name = "axel f",
        .data = AXELF_MOD,
    },
    {
        .len = 22480,
        .name = "dreams'n'fantasies",
        .data = cartoon_dreams_n_fantasies_mod,
    },
    {
        .len =  211606,
        .name = "IronMan",
        .data = IRONMAN_MOD,
    },
    {
        .len = 7600,
        .name = "deceased by disease",
        .data = nao_deceased_by_disease_mod,
    }
};

const mod_title_t *default_mod_title = &titles[0];

