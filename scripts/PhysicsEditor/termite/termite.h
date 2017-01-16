#pragma once
#include "termite/vec_math.h"

// {{global.body_id }}: {% for body in bodies %}{% for fixture in body.fixtures %}
static const termite::vec2_t k{{ global.body_id }}Verts{{ forloop.counter }}[] = {{% for point in fixture.hull %} {% if not forloop.first %}, {% endif %}termite::vec2f(float({{point.x}}), float({{point.y}})) {% endfor %}};
{% endfor %}
{% endfor %}
