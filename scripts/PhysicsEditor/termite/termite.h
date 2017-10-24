#pragma once
#include "termite/vec_math.h"

{% for body in bodies %}{% for fixture in body.fixtures %}
{% if fixture.isCircle %}
static const termite::vec3_t k{{ body.name }}Circle = vec3f({{ fixture.center.x|floatformat:2 }}f, {{ fixture.center.y|floatformat:2 }}f, {{ fixture.radius|floatformat:2 }}f);
{% else %}
static const termite::vec2_t k{{ body.name }}Verts{{ forloop.counter }}[] = {{% for point in fixture.hull %} {% if not forloop.first %}, {% endif %}termite::vec2f({{ point.x|floatformat:2 }}f, {{ point.y|floatformat:2 }}f) {% endfor %}};
{% endif %}
{% endfor %}
{% endfor %}
