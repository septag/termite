#pragma once
#include "termite/tmath.h"

{% for body in bodies %}{% for fixture in body.fixtures %}
{% if fixture.isCircle %}
static const tee::vec3_t k{{ body.name }}Circle = vec3({{ fixture.center.x|floatformat:2 }}f, {{ fixture.center.y|floatformat:2 }}f, {{ fixture.radius|floatformat:2 }}f);
{% else %}
static const tee::vec2_t k{{ body.name }}Verts{{ forloop.counter }}[] = {{% for point in fixture.hull %} {% if not forloop.first %}, {% endif %}tee::vec2({{ point.x|floatformat:2 }}f, {{ point.y|floatformat:2 }}f) {% endfor %}};
{% endif %}
{% endfor %}
{% endfor %}
