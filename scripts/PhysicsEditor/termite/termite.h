// {{global.body_id }}: {% for body in bodies %}{% for fixture in body.fixtures %}
static const vec2_t k{{ global.body_id }}Verts[] = {{% for point in fixture.hull %} {% if not forloop.first %}, {% endif %}vec2f(float({{point.x}}), float({{point.y}})) {% endfor %}};
{% endfor %}
{% endfor %}
