// GENERADO por Terraform (terraform/aws). NO lo edites ni lo subas a GitHub.
#pragma once

#define AWS_IOT_ENDPOINT "${endpoint}"
#define AWS_THING_NAME   "${cosa}"

static const char AWS_CERT_CA[] PROGMEM = R"EOF(
${ca}
)EOF";

static const char AWS_CERT_CRT[] PROGMEM = R"EOF(
${crt}
)EOF";

static const char AWS_CERT_KEY[] PROGMEM = R"EOF(
${llave}
)EOF";
