/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

package com.amazon.lumberyard.iap;

import android.app.Activity;
import android.os.Looper;
import android.util.Log;
import androidx.annotation.NonNull;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.ConsumeParams;
import com.android.billingclient.api.ConsumeResponseListener;
import com.android.billingclient.api.PendingPurchasesParams;
import com.android.billingclient.api.ProductDetailsResponseListener;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.PurchasesResponseListener;
import com.android.billingclient.api.PurchasesUpdatedListener;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryPurchasesParams;

//patched by CARBONATED 03.10.2025
/** @noinspection JavaJniMissingFunction*/
public class LumberyardInAppBilling implements PurchasesUpdatedListener
{
    public static class ProductDetails
    {
        public String m_title;
        public String m_description;
        public String m_productId;
        public String m_price;
        public String m_currencyCode;
        public String m_type;
        public long m_priceMicro;
    }

    public static class PurchasedProductDetails
    {
        public String m_productId;
        public String m_orderId;
        public String m_packageName;
        public String m_purchaseToken;
        public String m_signature;
        public long m_purchaseTime;
        public boolean m_isAutoRenewing;
        public String m_price;
        public String m_currencyCode;
        public long m_priceMicro;
    }

    private static final String s_tag = "LMBR";
    private static final String s_subTag = "(O3DEInAppPurchases) - ";

    private final Activity m_activity;
    private BillingClient m_billingClient;
    private boolean m_setupDone;

    private List<com.android.billingclient.api.ProductDetails> m_productDetailsList;

    private int m_numResponses;
    private final ArrayList<PurchasedProductDetails> m_queriedProductsList = new ArrayList<>();
    private int m_queryPurchasesResponseCount = 0;
    private final Object m_queryLock = new Object();
    private PurchasedProductDetails m_lastPurchasedProductDetails;

    public native void nativeProductInfoRetrieved(Object[] productDetails);
    public native void nativeNewProductPurchased(Object[] purchasedProductDetails);
    public native void nativePurchasedProductsRetrieved(Object[] purchasedProductDetails);
    public native void nativePurchaseConsumed(String purchaseToken);
    public native void nativePurchaseCancelled();
    public native void nativePurchaseFailed(int responseCode);

    public LumberyardInAppBilling(Activity activity)
    {
        m_setupDone = false;
        m_activity = activity;
        PendingPurchasesParams params = PendingPurchasesParams.newBuilder()
                .enableOneTimeProducts()
                .build();

        m_billingClient = BillingClient.newBuilder(m_activity)
                .setListener(this)
                .enablePendingPurchases(params)
                .build();

        if (!IsKindleDevice())
        {
            (new Thread(() ->
            {
                Looper.prepare();

                m_billingClient.startConnection(new BillingClientStateListener()
                {
                    @Override
                    public void onBillingSetupFinished(@NonNull BillingResult billingResult)
                    {
                        Log.d(s_tag, s_subTag + "Service connected.");

                        if (billingResult.getResponseCode() != BillingClient.BillingResponseCode.OK)
                        {
                            Log.d(s_tag, s_subTag + "Billing setup failed with response code: " + billingResult.getResponseCode());
                            return;
                        }

                        BillingResult subscriptionsSupportedResult = m_billingClient.isFeatureSupported(BillingClient.FeatureType.SUBSCRIPTIONS);
                        if (subscriptionsSupportedResult.getResponseCode() != BillingClient.BillingResponseCode.OK)
                        {
                            Log.d(s_tag, s_subTag + "Subscriptions not supported.");
                            return;
                        }

                        BillingResult subscriptionsUpdateSupportedResult = m_billingClient.isFeatureSupported(BillingClient.FeatureType.SUBSCRIPTIONS_UPDATE);
                        if (subscriptionsUpdateSupportedResult.getResponseCode() != BillingClient.BillingResponseCode.OK)
                        {
                            Log.d(s_tag, s_subTag + "Subscriptions update not supported.");
                            return;
                        }

                        m_setupDone = true;
                    }

                    @Override
                    public void onBillingServiceDisconnected()
                    {
                        Log.d(s_tag, s_subTag + "Service disconnected");
                    }
                });

                Looper.loop();
            })).start();
        }

        Log.d(s_tag, s_subTag + "Instance created");
    }

    public void UnbindService()
    {
        if (!m_setupDone)
        {
            Log.e(s_tag, s_subTag + "Not initialized!");
            return;
        }

        m_setupDone = false;

        m_billingClient.endConnection();
        m_billingClient = null;
    }

    @Override
    public void onPurchasesUpdated(BillingResult billingResult, List<Purchase> purchases)
    {
        final int responseCode = billingResult.getResponseCode();
        Log.d(s_tag, s_subTag + "onPurchasesUpdated responseCode = " + responseCode);

        if (responseCode == BillingClient.BillingResponseCode.OK && purchases != null)
        {
            purchases.forEach(purchase ->
            {
                ArrayList<PurchasedProductDetails> purchasedProducts = new ArrayList<>();
                ParsePurchasedProduct(purchase, purchasedProducts);
                if (!purchasedProducts.isEmpty())
                {
                    PurchasedProductDetails purchasedProduct = purchasedProducts.get(0);
                    if (purchasedProduct.m_orderId != null)
                    {
                        m_lastPurchasedProductDetails = purchasedProduct;
                        nativeNewProductPurchased(purchasedProducts.toArray());
                    }
                    else
                    {
                        Log.e(s_tag, s_subTag + "Payment is delayed");
                        nativePurchaseFailed(BillingClient.BillingResponseCode.SERVICE_TIMEOUT);
                    }
                }
                else
                {
                    Log.e(s_tag, s_subTag + "Unable to parse product");
                    nativePurchaseFailed(BillingClient.BillingResponseCode.ITEM_UNAVAILABLE);
                }
            });
        }
        else
        {
            if (responseCode == BillingClient.BillingResponseCode.USER_CANCELED)
            {
                Log.d(s_tag, s_subTag + "Purchase was cancelled by the user.");
                nativePurchaseCancelled();
            }
            else
            {
                Log.e(s_tag, s_subTag + "Purchase failed with error: " + billingResult.getDebugMessage());
                nativePurchaseFailed(responseCode);
            }
        }
    }

    public void QueryProductInfo(final String[] skuListArray)
    {
        if (!m_setupDone)
        {
            Log.e(s_tag, s_subTag + "Not initialized!");
            return;
        }

        m_numResponses = 0;
        final ArrayList<ProductDetails> responseList = new ArrayList<>();

        ProductDetailsResponseListener responseListener = (billingResult, productDetailsResult) ->
        {
            m_numResponses++;
            if (billingResult.getResponseCode() != BillingClient.BillingResponseCode.OK)
            {
                Log.e(s_tag, s_subTag + "Failed to query product details: " + billingResult.getDebugMessage());
                if (m_numResponses == 2)
                {
                    nativeProductInfoRetrieved(responseList.toArray());
                }
                return;
            }

            if (m_productDetailsList == null)
            {
                m_productDetailsList = new ArrayList<>();
            }
            List<com.android.billingclient.api.ProductDetails> productDetailsList = productDetailsResult.getProductDetailsList();
            m_productDetailsList.addAll(productDetailsList);

            for (com.android.billingclient.api.ProductDetails details : productDetailsList)
            {
                ProductDetails productDetails = new ProductDetails();
                productDetails.m_productId = details.getProductId();
                productDetails.m_title = details.getTitle();
                productDetails.m_description = details.getDescription();
                productDetails.m_type = details.getProductType();

                if (productDetails.m_type.equals(BillingClient.ProductType.INAPP))
                {
                    com.android.billingclient.api.ProductDetails.OneTimePurchaseOfferDetails offerDetails = details.getOneTimePurchaseOfferDetails();
                    if (offerDetails != null)
                    {
                        productDetails.m_price = offerDetails.getFormattedPrice();
                        productDetails.m_priceMicro = offerDetails.getPriceAmountMicros();
                        productDetails.m_currencyCode = offerDetails.getPriceCurrencyCode();
                    }
                }
                else
                {
                    if (productDetails.m_type.equals(BillingClient.ProductType.SUBS))
                    {
                        List<com.android.billingclient.api.ProductDetails.SubscriptionOfferDetails> subDetailsList = details.getSubscriptionOfferDetails();
                        if (subDetailsList != null && !subDetailsList.isEmpty())
                        {
                            com.android.billingclient.api.ProductDetails.SubscriptionOfferDetails subDetails = subDetailsList.get(0);
                            com.android.billingclient.api.ProductDetails.PricingPhase pricingPhase = subDetails.getPricingPhases().getPricingPhaseList().get(0);
                            productDetails.m_price = pricingPhase.getFormattedPrice();
                            productDetails.m_priceMicro = pricingPhase.getPriceAmountMicros();
                            productDetails.m_currencyCode = pricingPhase.getPriceCurrencyCode();
                        }
                    }
                }
                responseList.add(productDetails);
            }

            if (m_numResponses == 2)
            {
                nativeProductInfoRetrieved(responseList.toArray());
            }
        };

        List<QueryProductDetailsParams.Product> inAppProductList = new ArrayList<>();
        for(String sku : skuListArray)
        {
            inAppProductList.add(QueryProductDetailsParams.Product.newBuilder().setProductId(sku).setProductType(BillingClient.ProductType.INAPP).build());
        }
        QueryProductDetailsParams inAppParams = QueryProductDetailsParams.newBuilder().setProductList(inAppProductList).build();
        m_billingClient.queryProductDetailsAsync(inAppParams, responseListener);

        List<QueryProductDetailsParams.Product> subsProductList = new ArrayList<>();
        for(String sku : skuListArray)
        {
            subsProductList.add(QueryProductDetailsParams.Product.newBuilder().setProductId(sku).setProductType(BillingClient.ProductType.SUBS).build());
        }
        QueryProductDetailsParams subsParams = QueryProductDetailsParams.newBuilder().setProductList(subsProductList).build();
        m_billingClient.queryProductDetailsAsync(subsParams, responseListener);
    }

    public void PurchaseProduct(String productSku, String developerPayload, String productType)
    {
        if (!m_setupDone)
        {
            Log.e(s_tag, s_subTag + "Not initialized!");
            return;
        }

        com.android.billingclient.api.ProductDetails productDetailsToPurchase = null;
        if (m_productDetailsList != null)
        {
            for (com.android.billingclient.api.ProductDetails details : m_productDetailsList)
            {
                if (details.getProductId().equals(productSku))
                {
                    productDetailsToPurchase = details;
                    break;
                }
            }
        }

        if (productDetailsToPurchase == null)
        {
            Log.e(s_tag, s_subTag + "Product not found. Details may not be cached for SKU: " + productSku);
            return;
        }

        BillingFlowParams.Builder flowParamsBuilder = BillingFlowParams.newBuilder();

        if (developerPayload != null && !developerPayload.isEmpty())
        {
            flowParamsBuilder.setObfuscatedAccountId(developerPayload);
        }

        BillingFlowParams.ProductDetailsParams.Builder productDetailsParamsBuilder =
                BillingFlowParams.ProductDetailsParams.newBuilder()
                        .setProductDetails(productDetailsToPurchase);

        if (BillingClient.ProductType.SUBS.equals(productDetailsToPurchase.getProductType()))
        {
            List<com.android.billingclient.api.ProductDetails.SubscriptionOfferDetails> offerDetailsList = productDetailsToPurchase.getSubscriptionOfferDetails();
            if (offerDetailsList != null && !offerDetailsList.isEmpty())
            {
                String offerToken = offerDetailsList.get(0).getOfferToken();
                productDetailsParamsBuilder.setOfferToken(offerToken);
            }
            else
            {
                Log.e(s_tag, s_subTag + "No subscription offers found for SKU: " + productSku);
                return;
            }
        }

        flowParamsBuilder.setProductDetailsParamsList(Collections.singletonList(productDetailsParamsBuilder.build()));

        BillingFlowParams flowParams = flowParamsBuilder.build();
        BillingResult billingResult = m_billingClient.launchBillingFlow(m_activity, flowParams);

        if (billingResult.getResponseCode() != BillingClient.BillingResponseCode.OK)
        {
            Log.e(s_tag, s_subTag + "Failed to launch billing flow: " + billingResult.getDebugMessage());
            return;
        }

        Log.d(s_tag, s_subTag + "Purchase flow initiated.");
    }

    public void QueryPurchasedProducts()
    {
        if (!m_setupDone)
        {
            Log.e(s_tag, s_subTag + "Not initialized!");
            return;
        }

        synchronized (m_queryLock)
        {
            m_queryPurchasesResponseCount = 0;
            m_queriedProductsList.clear();
        }

        PurchasesResponseListener listener = (billingResult, purchases) ->
        {
            synchronized (m_queryLock)
            {
                if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK)
                {
                    ParsePurchasedProducts(purchases, m_queriedProductsList);
                }
                else
                {
                    Log.e(s_tag, s_subTag + "Failed to query purchases: " + billingResult.getDebugMessage());
                }

                m_queryPurchasesResponseCount++;

                if (m_queryPurchasesResponseCount == 2)
                {
                    nativePurchasedProductsRetrieved(m_queriedProductsList.toArray());
                }
            }
        };

        m_billingClient.queryPurchasesAsync(QueryPurchasesParams.newBuilder().setProductType(BillingClient.ProductType.INAPP).build(), listener);
        m_billingClient.queryPurchasesAsync(QueryPurchasesParams.newBuilder().setProductType(BillingClient.ProductType.SUBS).build(), listener);
    }

    public void ConsumePurchase(final String purchaseToken)
    {
        if (!m_setupDone)
        {
            Log.e(s_tag, s_subTag + "Not initialized!");
            return;
        }

        ConsumeResponseListener listener = (billingResult, token) ->
        {
            if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK)
            {
                Log.d(s_tag, s_subTag + "Purchase consumed successfully: " + token);
                nativePurchaseConsumed(token);
            }
            else
            {
                Log.e(s_tag, s_subTag + "Error consuming purchase: " + billingResult.getDebugMessage());
            }
        };

        ConsumeParams consumeParams = ConsumeParams.newBuilder().setPurchaseToken(purchaseToken).build();
        m_billingClient.consumeAsync(consumeParams, listener);
    }

    public String GetLastTransactionReceipt()
    {
        if (m_lastPurchasedProductDetails == null)
        {
            return "";
        }
        return m_lastPurchasedProductDetails.m_purchaseToken;
    }

    private void ParsePurchasedProducts(List<Purchase> purchases, ArrayList<PurchasedProductDetails> purchasedProducts)
    {
        if (purchases == null)
        {
            return;
        }

        for (Purchase purchase : purchases)
        {
            ParsePurchasedProduct(purchase, purchasedProducts);
        }
    }

    private void ParsePurchasedProduct(Purchase purchase, ArrayList<PurchasedProductDetails> purchasedProducts)
    {
        if (purchase == null)
        {
            return;
        }

        for (String productId : purchase.getProducts())
        {
            com.android.billingclient.api.ProductDetails.OneTimePurchaseOfferDetails purchaseOfferDetails = null;
            for (com.android.billingclient.api.ProductDetails details : m_productDetailsList)
            {
                if (details.getProductId().equals(productId))
                {
                    purchaseOfferDetails = details.getOneTimePurchaseOfferDetails();
                    break;
                }
            }

            PurchasedProductDetails purchasedProductDetails = new PurchasedProductDetails();
            purchasedProductDetails.m_productId = productId;
            purchasedProductDetails.m_orderId = purchase.getOrderId();
            purchasedProductDetails.m_packageName = purchase.getPackageName();
            purchasedProductDetails.m_purchaseToken = purchase.getPurchaseToken();
            purchasedProductDetails.m_signature = purchase.getSignature();
            purchasedProductDetails.m_purchaseTime = purchase.getPurchaseTime();
            purchasedProductDetails.m_isAutoRenewing = purchase.isAutoRenewing();
            if (purchaseOfferDetails != null)
            {
                purchasedProductDetails.m_price = purchaseOfferDetails.getFormattedPrice();
                purchasedProductDetails.m_currencyCode = purchaseOfferDetails.getPriceCurrencyCode();
                purchasedProductDetails.m_priceMicro = purchaseOfferDetails.getPriceAmountMicros();
            }
            else
            {
                Log.e(s_tag, s_subTag + "Can't find product '" + productId + "' details");
            }

            purchasedProducts.add(purchasedProductDetails);
        }
    }

    private boolean IsKindleDevice()
    {
        return android.os.Build.MANUFACTURER.equals("Amazon");
    }
}
