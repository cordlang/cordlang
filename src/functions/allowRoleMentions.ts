export function allowRoleMentions(params: string[]): string[] {
    // Parámetros: [Role IDs;...] separados por punto y coma
    return params.map(roleId => roleId.trim()).filter(id => id);
  }
  