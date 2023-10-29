#include "step_wedge.h"

#include <FreeRTOS.h>
#include <string.h>
#include <math.h>
#include "util.h"

static const step_wedge_t stock_wedges[] = {
    { "Stouffer T1015", 0.05F, 0.15F, 10 },
    { "Stouffer T1415", 0.05F, 0.15F, 14 },
    { "Stouffer T2115", 0.05F, 0.15F, 21 },
    { "Stouffer T2120", 0.05F, 0.20F, 21 },
    { "Stouffer T3110", 0.05F, 0.10F, 31 },
    { "Stouffer T4105", 0.05F, 0.05F, 41 },
    { "Stouffer T4110", 0.05F, 0.10F, 41 },
    { "Kodak Step Tablet No.2", 0.05F, 0.15F, 21 }
};

#define STOCK_WEDGES_SIZE 8

const char *step_wedge_stock_name(size_t index)
{
    if (index < STOCK_WEDGES_SIZE) {
        return stock_wedges[index].name;
    } else {
        return NULL;
    }
}

int step_wedge_stock_step_count(size_t index)
{
    if (index < STOCK_WEDGES_SIZE) {
        return stock_wedges[index].step_count;
    } else {
        return 0;
    }
}

int step_wedge_stock_count()
{
    return STOCK_WEDGES_SIZE;
}

step_wedge_t *step_wedge_create(uint32_t step_count)
{
    if (step_count < MIN_STEP_WEDGE_STEP_COUNT || step_count > MAX_STEP_WEDGE_STEP_COUNT) {
        return NULL;
    }

    step_wedge_t *wedge = pvPortMalloc(sizeof(step_wedge_t) + sizeof(float) * step_count);
    if (wedge) {
        memset(wedge->name, 0, sizeof(wedge->name));
        wedge->base_density = 0.05F;
        wedge->density_increment = 0.15F;
        wedge->step_count = step_count;
        for (size_t i = 0; i < step_count; i++) {
            wedge->step_density[i] = NAN;
        }
    }
    return wedge;
}

step_wedge_t *step_wedge_create_from_stock(size_t index)
{
    if (index >= STOCK_WEDGES_SIZE) {
        return NULL;
    }

    const step_wedge_t *stock_wedge = &stock_wedges[index];
    step_wedge_t *wedge = pvPortMalloc(sizeof(step_wedge_t) + sizeof(float) * stock_wedge->step_count);
    if (wedge) {
        strcpy(wedge->name, stock_wedge->name);
        wedge->base_density = stock_wedge->base_density;
        wedge->density_increment = stock_wedge->density_increment;
        wedge->step_count = stock_wedge->step_count;
        for (size_t i = 0; i < wedge->step_count; i++) {
            wedge->step_density[i] = NAN;
        }
    }

    return wedge;
}

step_wedge_t *step_wedge_copy(const step_wedge_t *source_wedge, uint32_t step_count)
{
    if (!source_wedge || step_count < MIN_STEP_WEDGE_STEP_COUNT || step_count > MAX_STEP_WEDGE_STEP_COUNT) {
        return NULL;
    }

    step_wedge_t *wedge = pvPortMalloc(sizeof(step_wedge_t) + sizeof(float) * step_count);
    if (wedge) {
        strcpy(wedge->name, source_wedge->name);
        wedge->base_density = source_wedge->base_density;
        wedge->density_increment = source_wedge->density_increment;
        wedge->step_count = step_count;
        for (size_t i = 0; i < step_count; i++) {
            if (i < source_wedge->step_count) {
                wedge->step_density[i] = source_wedge->step_density[i];
            } else {
                wedge->step_density[i] = NAN;
            }
        }
    }
    return wedge;
}

void step_wedge_free(step_wedge_t *wedge)
{
    if (wedge) {
        vPortFree(wedge);
    }
}

float step_wedge_get_density(const step_wedge_t *wedge, uint32_t step)
{
    if (!wedge || step >= wedge->step_count) {
        return NAN;
    }

    if (isnormal(wedge->step_density[step]) || fpclassify(wedge->step_density[step]) == FP_ZERO) {
        return wedge->step_density[step];
    } else {
        return wedge->base_density + (wedge->density_increment * step);
    }
}

bool step_wedge_is_valid(const step_wedge_t *wedge)
{
    if (!wedge) {
        return false;
    }

    /* Base density must be a valid number */
    if (!(isnormal(wedge->base_density) || fpclassify(wedge->base_density) == FP_ZERO)) {
        return false;
    }

    /* Density increment must be a valid non-zero number */
    if (!isnormal(wedge->density_increment)) {
        return false;
    }

    /* Density increment must be positive and within our display threshold */
    if (wedge->density_increment < 0.01F) {
        return false;
    }

    /* Step count must be within the valid range */
    if (wedge->step_count < MIN_STEP_WEDGE_STEP_COUNT || wedge->step_count > MAX_STEP_WEDGE_STEP_COUNT) {
        return false;
    }

    /* Validate the list of step densities */
    for (uint32_t i = 1; i < wedge->step_count; i++) {
        float prev_val = step_wedge_get_density(wedge, i - 1);
        float cur_val = step_wedge_get_density(wedge, i);

        /* Values must be increasing */
        if (cur_val <= prev_val) {
            return false;
        }

        /* Values may not be equivalent */
        if (fabsf(cur_val - prev_val) < 0.01F) {
            return false;
        }
    }

    return true;
}

bool step_wedge_compare(const step_wedge_t *wedge1, const step_wedge_t *wedge2)
{
    /* Both are the same pointer */
    if (wedge1 == wedge2) {
        return true;
    }

    /* Both are null */
    if (!wedge1 && !wedge2) {
        return true;
    }

    /* One is null and the other is not */
    if ((wedge1 && !wedge2) || (!wedge1 && wedge2)) {
        return false;
    }

    /* Compare the name strings */
    if (strncmp(wedge1->name, wedge2->name, sizeof(wedge1->name)) != 0) {
        return false;
    }

    /* Compare the fields */
    if (fabsf(wedge1->base_density - wedge2->base_density) > 0.001F) {
        return false;
    }
    if (fabsf(wedge1->density_increment - wedge2->density_increment) > 0.001F) {
        return false;
    }
    if (wedge1->step_count != wedge2->step_count) {
        return false;
    }

    /* Compare the step density list */
    for (size_t i = 0; i < wedge1->step_count; i++) {
        if (is_valid_number(wedge1->step_density[i]) && is_valid_number(wedge2->step_density[i])
            && fabsf(wedge1->step_density[i] - wedge2->step_density[i]) > 0.001F) {
            return false;
        } else if (fpclassify(wedge1->step_density[i]) != fpclassify(wedge2->step_density[i])) {
            return false;
        }
    }

    return true;
}
