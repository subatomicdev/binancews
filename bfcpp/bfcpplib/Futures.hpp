#ifndef __BINANCE_FUTURES_HPP 
#define __BINANCE_FUTURES_HPP


#include <string>
#include <vector>
#include <functional>
#include <map>
#include <any>
#include <set>
#include <cpprest/ws_client.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>
#include <openssl/hmac.h>
#include "IntervalTimer.hpp"
#include "bfcppCommon.hpp"


namespace bfcpp
{
  /// <summary>
  /// Access the USD-M Future's market. You must have a Futures account.
  /// The APis keys must be enabled for Futures in the API Management settings. 
  /// If you created the API key before you created your Futures account, you must create a new API key.
  /// </summary>
  class UsdFuturesMarket
  {
    inline const static string DefaultReceiveWindwow = "5000";

    // default receive windows. these can be change with setReceiveWindow()
    inline static map<RestCall, string> ReceiveWindowMap =
    {
        {RestCall::NewOrder,    DefaultReceiveWindwow},
        {RestCall::ListenKey,   DefaultReceiveWindwow},    // no affect for RestCall::ListenKey, here for completion
        {RestCall::CancelOrder, DefaultReceiveWindwow},
        {RestCall::AllOrders,   DefaultReceiveWindwow},
        {RestCall::AccountInfo, DefaultReceiveWindwow},
        {RestCall::AccountBalance, DefaultReceiveWindwow},
        {RestCall::TakerBuySellVolume, DefaultReceiveWindwow},
        {RestCall::KlineCandles, DefaultReceiveWindwow},
        {RestCall::Ping, DefaultReceiveWindwow}
    };


  protected:
    UsdFuturesMarket(MarketType mt, const string& exchangeUri, const ApiAccess& access) : m_marketType(mt), m_exchangeBaseUri(exchangeUri), m_apiAccess(access)
    {
      m_monitorId = 1;
    }


  public:
    UsdFuturesMarket(const ApiAccess& access = {}) : UsdFuturesMarket(MarketType::Futures, FuturestWebSockUri, access)
    {

    }


    virtual ~UsdFuturesMarket()
    {
      disconnect();
    }


    /// <summary>
    /// This measures the time it takes to send a "PING" request to the exchange and receive a reply.
    /// It includes near zero processing time by bfcpp, so the returned duration can be assumed to be network latency and Binance's processing time.
    /// Testing has seen this latency range from 300ms to 750ms between calls, whilst an ICMP ping is 18ms.
    /// See https://binance-docs.github.io/apidocs/futures/en/#test-connectivity.
    /// </summary>
    /// <returns>The latency in milliseconds</returns>
    std::chrono::milliseconds ping()
    {
      try
      {
        web::http::client::http_client client{ web::uri { utility::conversions::to_string_t(getApiUri(m_marketType)) } };

        auto request = createHttpRequest(web::http::methods::POST, getApiPath(m_marketType, RestCall::NewOrder) + "?" + createQueryString({}, RestCall::Ping, false));

        auto send = Clock::now();
        auto rcv = client.request(std::move(request)).then([](web::http::http_response response) { return Clock::now(); }).get();

        return std::chrono::duration_cast<std::chrono::milliseconds>(rcv - send);
      }
      catch (const pplx::task_canceled tc)
      {
        throw BfcppDisconnectException("ping");
      }
      catch (const std::exception ex)
      {
        throw BfcppException(ex.what());
      }
    }


    /// <summary>
    /// Futures Only. Receives data from here: https://binance-docs.github.io/apidocs/futures/en/#mark-price-stream-for-all-market
    /// </summary>
    /// <param name = "onData">Your callback function. See this classes docs for an explanation </param>
    /// <returns></returns>
    MonitorToken monitorMarkPrice(std::function<void(BinanceKeyMultiValueData)> onData);


    /// <summary>
    /// Monitor data on the spot market.
    /// </summary>
    /// <param name="apiKey"></param>
    /// <param name="onData"></param>
    /// <param name="mode"></param>
    /// <returns></returns>
    MonitorToken monitorUserData(std::function<void(UsdFutureUserData)> onData);


    // --- monitor functions


    /// <summary>
    /// Receives from the miniTicker stream for all symbols
    /// Updates every 1000ms (limited by the Binance API).
    /// </summary>
    /// <param name="onData">Your callback function. See this classes docs for an explanation</param>
    /// <returns>A MonitorToken. If MonitorToken::isValid() is a problem occured.</returns>
    MonitorToken monitorMiniTicker(std::function<void(BinanceKeyMultiValueData)> onData);


    /// <summary>
    /// Receives from the 
    /// </summary>
    /// <param name="symbol"></param>
    /// <param name="onData"></param>
    /// <returns></returns>
    MonitorToken monitorKlineCandlestickStream(const string& symbol, const string& interval, std::function<void(BinanceKeyMultiValueData)> onData);


    /// <summary>
    /// Receives from the symbol mini ticker
    /// Updated every 1000ms (limited by the Binance API).
    /// </summary>
    /// <param name="symbol">The symbtol to monitor</param>
    /// <param name = "onData">Your callback function.See this classes docs for an explanation< / param>
    /// <returns></returns>
    MonitorToken monitorSymbol(const string& symbol, std::function<void(BinanceKeyValueData)> onData);


    /// <summary>
    /// Receives from the Individual Symbol Book stream for a given symbol.
    /// </summary>
    /// <param name="symbol">The symbol</param>
    /// <param name = "onData">Your callback function.See this classes docs for an explanation< / param>
    /// <returns></returns>
    MonitorToken monitorSymbolBookStream(const string& symbol, std::function<void(BinanceKeyValueData)> onData);


    /// <summary>
    /// See https://binance-docs.github.io/apidocs/futures/en/#account-information-v2-user_data
    /// </summary>
    /// <returns></returns>
    AccountInformation accountInformation();


    /// <summary>
    /// See https://binance-docs.github.io/apidocs/futures/en/#futures-account-balance-v2-user_data
    /// </summary>
    /// <returns></returns>
    AccountBalance accountBalance();


    /// <summary>
    /// See See https://binance-docs.github.io/apidocs/futures/en/#long-short-ratio
    /// </summary>
    /// <param name="query"></param>
    /// <returns></returns>
    virtual TakerBuySellVolume takerBuySellVolume(map<string, string>&& query);


    /// <summary>
    /// Becareful with the LIMIT value, it determines the weight of the API call and you want to only handle
    /// the data you require. Default LIMIT is 500.
    /// See https://binance-docs.github.io/apidocs/futures/en/#kline-candlestick-data
    /// </summary>
    /// <param name="query"></param>
    /// <returns></returns>
    KlineCandlestick klines(map<string, string>&& query);

    
    
    // --- order management


    /// <summary>
    /// Create a new order. 
    /// 
    /// The NewOrderResult is returned which contains the response from the Rest call,
    /// see https://binance-docs.github.io/apidocs/futures/en/#new-order-trade.
    /// 
    /// If the order is successful, the User Data Stream will be updated.
    /// 
    /// Use the priceTransform() function to make the price value suitable.
    /// </summary>
    /// <param name="order">Order params, see link above.</param>
    /// <returns>See 'response' Rest, see link above.</returns>
    NewOrderResult newOrder(map<string, string>&& order, const bool async = false);


    /// <summary>
    /// Returns all orders. What is returned is dependent on the status and order time, read:
    /// https://binance-docs.github.io/apidocs/futures/en/#all-orders-user_data
    /// </summary>
    /// <param name="query"></param>
    /// <returns></returns>
    AllOrdersResult allOrders(map<string, string>&& query);


    /// <summary>
    /// 
    /// </summary>
    /// <param name="order"></param>
    /// <returns></returns>
    CancelOrderResult cancelOrder(map<string, string>&& order);


    /// <summary>
    /// Close stream for the given token.
    /// </summary>
    /// <param name="mt"></param>
    void cancelMonitor(const MonitorToken& mt)
    {
      if (auto it = m_idToSession.find(mt.id); it != m_idToSession.end())
      {
        disconnect(mt, true);
      }
    }


    /// <summary>
    /// Close all streams.
    /// </summary>
    void cancelMonitors()
    {
      disconnect();
    }


    /// <summary>
    /// Set the API key(s). 
    /// All calls require the API key. You only need secret key set if using a call which requires signing, such as newOrder.
    /// </summary>
    /// <param name="apiKey"></param>
    /// <param name="secretKey"></param>
    void setApiKeys(const ApiAccess access = {})
    {
      m_apiAccess = access;
    }


    /// <summary>
    /// Sets the receive window. For defaults see member ReceiveWindowMap.
    /// Read about receive window in the "Timing Security" section at: https://binance-docs.github.io/apidocs/futures/en/#endpoint-security-type
    /// Note the receive window for RestCall::ListenKey has no affect
    /// </summary>
    /// <param name="call">The call for which this will set the time</param>
    /// <param name="ms">time in milliseconds</param>
    void setReceiveWindow(const RestCall call, std::chrono::milliseconds ms)
    {
      ReceiveWindowMap[call] = std::to_string(ms.count());
    }



  private:

    constexpr bool mustConvertStringT()
    {
      return std::is_same_v<utility::string_t, MarketStringType> == false;
    }


    void onUserDataTimer()
    {
      auto request = createHttpRequest(web::http::methods::PUT, getApiPath(m_marketType, RestCall::ListenKey));

      web::http::client::http_client client{ web::uri{utility::conversions::to_string_t(getApiUri(m_marketType))} };
      client.request(std::move(request)).then([this](web::http::http_response response)
      {
        if (response.status_code() != web::http::status_codes::OK)
        {
          throw BfcppException("ERROR : keepalive for listen key failed");
        }
      }).wait();
    }


    void handleUserDataStream(shared_ptr<WebSocketSession> session, std::function<void(UsdFutureUserData)> onData)
    {
      while (!session->getCancelToken().is_canceled())
      {
        try
        {
          auto  rcv = session->client.receive().then([=, token = session->getCancelToken()](pplx::task<ws::client::websocket_incoming_message> websocketInMessage)
          {
            if (!token.is_canceled())
            {
              try
              {
                std::string strMsg;
                websocketInMessage.get().extract_string().then([=, &strMsg, cancelToken = session->getCancelToken()](pplx::task<std::string> str_tsk)
                {
                  try
                  {
                    if (!cancelToken.is_canceled())
                      strMsg = str_tsk.get();
                  }
                  catch (...)
                  {
                    throw;
                  }
                }, session->getCancelToken()).wait();


                if (!strMsg.empty())
                {
                  std::error_code errCode;
                  if (auto json = web::json::value::parse(strMsg, errCode); errCode.value() == 0)
                  {
                    extractUsdFuturesUserData(session, std::move(json));
                  }
                  else
                  {
                    throw BfcppException("Invalid json: " + strMsg); // TODO should this be an exception or just ignore?
                  }
                }
              }
              catch (pplx::task_canceled tc)
              {
                throw BfcppDisconnectException(session->uri);
              }
              catch (std::exception ex)
              {
                throw;
              }
            }
            else
            {
              pplx::cancel_current_task();
            }

          }, session->getCancelToken()).wait();
        }
        catch (...)
        {
          throw;
        }
      }

      pplx::cancel_current_task();
    }


    void extractUsdFuturesUserData(shared_ptr<WebSocketSession> session, web::json::value&& jsonVal);
  

    shared_ptr<WebSocketSession> connect(const string& uri);
    
    void disconnect(const MonitorToken& mt, const bool deleteSession);
    void disconnect();


    std::tuple<MonitorToken, shared_ptr<WebSocketSession>> createMonitor(const string& uri, const JsonKeys& keys, const string& arrayKey = {});

    bool createListenKey(const MarketType marketType);


    string createQueryString(map<string, string>&& queryValues, const RestCall call, const bool sign)
    {
      stringstream ss;

      // can leave a trailing '&' without borking the internets
      std::for_each(queryValues.begin(), queryValues.end(), [&ss](auto& it)
      {
        ss << std::move(it.first) << "=" << std::move(it.second) << "&"; // TODO doing a move() with operator<< here  - any advantage?
      });

      if (sign)
      {
        ss << "recvWindow=" << ReceiveWindowMap.at(call) << "&timestamp=" << getTimestamp();

        string qs = ss.str();
        return qs + "&signature=" + (createSignature(m_apiAccess.secretKey, qs));
      }
      else
      {
        return ss.str();
      }
    }


    web::http::http_request createHttpRequest(const web::http::method method, string uri)
    {
      web::http::http_request request{ method };
      request.headers().add(utility::conversions::to_string_t(HeaderApiKeyName), utility::conversions::to_string_t(m_apiAccess.apiKey));
      request.headers().add(utility::conversions::to_string_t(ContentTypeName), utility::conversions::to_string_t("application/json"));
      request.headers().add(utility::conversions::to_string_t(ClientSDKVersionName), utility::conversions::to_string_t("binance_futures_cpp"));
      request.set_request_uri(web::uri{ utility::conversions::to_string_t(uri) });
      return request;
    }


    MonitorToken createReceiveTask(shared_ptr<WebSocketSession> session, std::function<void(ws::client::websocket_incoming_message, shared_ptr<WebSocketSession>, const JsonKeys&, const string&)> extractFunc, const JsonKeys& keys, const string& arrayKey);


    void extractKeys(ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session, const JsonKeys& keys, const string& arrayKey = {});

    
    template<class RestResultT>
    Concurrency::task<RestResultT> sendRestRequest(const RestCall call, const web::http::method method, const bool sign, const MarketType mt, std::function<RestResultT(web::http::http_response)> handler, map<string, string>&& query = {})
    {
      try
      {
        string queryString{ createQueryString(std::move(query), call, true) };

        auto request = createHttpRequest(method, getApiPath(mt, call) + "?" + queryString);

        web::http::client::http_client client{ web::uri { utility::conversions::to_string_t(getApiUri(mt)) } };
        
        return client.request(std::move(request)).then([handler](web::http::http_response response)
        {
          if (response.status_code() == web::http::status_codes::OK)
          {
            return handler(response);
          }
          else
          {
            return createInvalidRestResult<RestResultT>(utility::conversions::to_utf8string(response.extract_json().get().serialize()));
          }
        });
      }
      catch (const pplx::task_canceled tc)
      {
        throw;
      }
      catch (const std::exception ex)
      {
        throw;
      }
    }


  private:
    shared_ptr<WebSocketSession> m_session;
    MarketType m_marketType;

    vector<shared_ptr<WebSocketSession>> m_sessions;
    map<MonitorTokenId, shared_ptr<WebSocketSession>> m_idToSession;

    std::atomic_size_t m_monitorId;
    string m_exchangeBaseUri;
    std::atomic_bool m_connected;
    std::atomic_bool m_running;
    string m_listenKey;
    ApiAccess m_apiAccess;

    IntervalTimer m_userDataStreamTimer;
};



  /// <summary>
  ///  Uses Binance's Test Net market. Most endpoints are available, including data streams for orders. 
  ///  See:  https://testnet.binancefuture.com/en/futures/BTC_USDT
  ///  To use the TestNet you must:
  ///     1) Create/login to an account on https://testnet.binancefuture.com/en/futures/BTC_USDT
  ///     2) Unlike the 'real' accounts, there's no API Management page, instead there's an "API Key" section at the bottom of the trading page, to the right of Positions, Open Orders, etc
  /// </summary>
  class UsdFuturesTestMarket : public UsdFuturesMarket
  {
  public:
    UsdFuturesTestMarket(const ApiAccess& access = {}) : UsdFuturesMarket(MarketType::FuturesTest, TestFuturestWebSockUri, access)
    {

    }

    virtual ~UsdFuturesTestMarket()
    {
    }


  public:
    virtual TakerBuySellVolume takerBuySellVolume(map<string, string>&& query)
    {
      throw BfcppException("Function unavailable on Testnet");
    }
  };

}

#endif
