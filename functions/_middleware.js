export async function onRequest(context) {
  const url = new URL(context.request.url);

  if (url.hostname === 'armagetronad-wasm.pages.dev') {
    url.hostname = 'armagetronad.enrichgenius.com';
    return Response.redirect(url.toString(), 308);
  }

  return context.next();
}
